import json
import networkx as nx
import random
import math
import time
import multiprocessing as mp
from functools import partial
from statistics import mean, stdev

class RefinedBatteryCellOptimizer:
    def __init__(self, original_data, initial_solution):
        self.cells = original_data
        self.initial_solution = initial_solution
        self.graph = self.create_graph()
        self.edge_cells = self.identify_edge_cells()  # Move this line before calculate_score
        self.best_solution = initial_solution
        self.best_score = self.calculate_score(initial_solution)

    def create_graph(self):
        G = nx.Graph()
        for cell in self.cells:
            n = cell['n']
            G.add_node(n)
            for i in range(6):
                if str(i) in cell:
                    neighbor = cell[str(i)]
                    G.add_edge(n, neighbor)
        return G

    def identify_edge_cells(self):
        return [cell['n'] for cell in self.cells if sum(1 for i in range(6) if str(i) in cell) < 6]

    def is_valid_group(self, group):
        return len(group) == 14 and nx.is_connected(self.graph.subgraph(group))

    def calculate_surface_area(self, group1, group2):
        return sum(1 for u in group1 for v in group2 if self.graph.has_edge(u, v))

    def calculate_group_spread(self, group):
        subgraph = self.graph.subgraph(group)
        distances = dict(nx.all_pairs_shortest_path_length(subgraph))
        max_distance = max(max(d.values()) for d in distances.values())
        avg_distance = sum(sum(d.values()) for d in distances.values()) / (len(group) * (len(group) - 1))
        return max_distance + avg_distance

    def calculate_edge_cooling_score(self, groups):
        edge_cooling_score = 0
        for group in groups:
            edge_cells_in_group = sum(1 for cell in group if cell in self.edge_cells)
            edge_cooling_score += edge_cells_in_group
        return edge_cooling_score

    def calculate_interconnection_balance(self, surface_areas):
        return -stdev(surface_areas)  # Negative because we want to maximize this (minimize standard deviation)

    def calculate_score(self, solution):
        groups = [[] for _ in range(10)]
        for cell, group in solution.items():
            groups[int(group) - 1].append(int(cell))
        
        surface_areas = [self.calculate_surface_area(groups[i], groups[i+1]) for i in range(9)]
        total_surface_area = sum(surface_areas)
        spread = sum(self.calculate_group_spread(group) for group in groups)
        edge_cooling_score = self.calculate_edge_cooling_score(groups)
        interconnection_balance = self.calculate_interconnection_balance(surface_areas)
        
        # Adjusted weights
        return (2 * total_surface_area +
                50 * interconnection_balance +
                10 * edge_cooling_score -
                3 * spread)

    def swap_cells(self, solution, cell1, cell2):
        new_solution = solution.copy()
        new_solution[str(cell1)], new_solution[str(cell2)] = new_solution[str(cell2)], new_solution[str(cell1)]
        return new_solution

    def is_valid_solution(self, solution):
        groups = [[] for _ in range(10)]
        for cell, group in solution.items():
            groups[int(group) - 1].append(int(cell))
        
        for group in groups:
            if not self.is_valid_group(group):
                return False
        
        if set(groups[0]) != set(range(127, 141)):
            return False
        if not all(cell in groups[9] for cell in [1, 2, 6]):
            return False
        
        return True

    def simulated_annealing(self, initial_temp=100, cooling_rate=0.99995, iterations=2000000):
        current_solution = self.best_solution.copy()
        current_score = self.calculate_score(current_solution)
        temperature = initial_temp

        for i in range(iterations):
            cell1, cell2 = random.sample(list(current_solution.keys()), 2)
            if current_solution[cell1] in ['1', '10'] or current_solution[cell2] in ['1', '10']:
                continue

            new_solution = self.swap_cells(current_solution, cell1, cell2)
            if not self.is_valid_solution(new_solution):
                continue

            new_score = self.calculate_score(new_solution)
            delta = new_score - current_score

            if delta > 0 or random.random() < math.exp(delta / temperature):
                current_solution = new_solution
                current_score = new_score

                if current_score > self.best_score:
                    self.best_solution = current_solution.copy()
                    self.best_score = current_score
                    print(f"New best score found: {self.best_score}")

            temperature *= cooling_rate

            if i % 10000 == 0:
                print(f"Iteration {i}, Temperature: {temperature:.2f}, Current Score: {current_score}")

        return self.best_solution, self.best_score

def optimize_wrapper(original_data, initial_solution, seed):
    random.seed(seed)
    optimizer = RefinedBatteryCellOptimizer(original_data, initial_solution)
    return optimizer.simulated_annealing()

def main():
    start_time = time.time()
    print("Starting refined battery cell cluster optimization...")

    original_data = load_json('paste.txt')
    initial_solution = load_json('initial_solution.json')

    num_processes = mp.cpu_count()
    pool = mp.Pool(processes=num_processes)

    num_runs = 20  # Number of parallel optimization runs
    partial_optimize = partial(optimize_wrapper, original_data, initial_solution)
    results = pool.map(partial_optimize, range(num_runs))

    pool.close()
    pool.join()

    best_solution, best_score = max(results, key=lambda x: x[1])

    print(f"\nOptimization complete. Best score: {best_score}")
    print(f"Total runtime: {(time.time() - start_time) / 3600:.2f} hours")

    # Save the best solution to a file
    with open('refined_optimized_battery_clusters.json', 'w') as f:
        json.dump(best_solution, f)
    print("Best solution saved to refined_optimized_battery_clusters.json")

def load_json(filename):
    with open(filename, 'r') as file:
        return json.load(file)

if __name__ == "__main__":
    main()
