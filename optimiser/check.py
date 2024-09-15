import json
import networkx as nx
import sys
from statistics import mean, stdev

def load_json(filename):
    with open(filename, 'r') as file:
        return json.load(file)

def create_graph(cells_data):
    G = nx.Graph()
    for cell in cells_data:
        n = cell['n']
        G.add_node(n, x=cell.get('x', 0), y=cell.get('y', 0))
        for i in range(6):
            if str(i) in cell:
                neighbor = cell[str(i)]
                G.add_edge(n, neighbor)
    return G

def calculate_current_path_length(graph, groups):
    centroids = []
    for group in groups:
        x_coords = [graph.nodes[cell]['x'] for cell in group]
        y_coords = [graph.nodes[cell]['y'] for cell in group]
        centroids.append((mean(x_coords), mean(y_coords)))
    
    total_distance = sum(
        ((centroids[i][0] - centroids[i+1][0])**2 + (centroids[i][1] - centroids[i+1][1])**2)**0.5
        for i in range(len(centroids) - 1)
    )
    return total_distance

def calculate_degradation_balance(graph, groups):
    group_stress = []
    for i, group in enumerate(groups):
        if i == 0:
            stress = sum(1 for u in group for v in groups[i+1] if graph.has_edge(u, v))
        elif i == len(groups) - 1:
            stress = sum(1 for u in group for v in groups[i-1] if graph.has_edge(u, v))
        else:
            stress = sum(1 for u in group for v in groups[i-1] + groups[i+1] if graph.has_edge(u, v))
        group_stress.append(stress)
    return stdev(group_stress)

def calculate_edge_cooling_score(graph, groups):
    edge_cells = [node for node, degree in graph.degree() if degree < 6]
    return sum(sum(1 for cell in group if cell in edge_cells) for group in groups)

def validate_and_analyze_solution(original_data, solution_data, num_groups=10, cells_per_group=14):
    graph = create_graph(original_data)
    groups = [[] for _ in range(num_groups)]
    
    for cell, group in solution_data.items():
        groups[int(group) - 1].append(int(cell))
    
    # Check constraints
    valid_groups = []
    for i, group in enumerate(groups):
        if len(group) != cells_per_group or not nx.is_connected(graph.subgraph(group)):
            print(f"Error: Group {i+1} is invalid")
            return False
        valid_groups.append(str(i+1))
    
    print(f"Checking Groups: {', '.join(valid_groups)} are valid")
    
    # Check initial constraints
    if set(groups[0]) != set(range(127, 141)):
        print("Error: Group 1 does not match the initial constraint (cells 127-140)")
        return False
    
    if not all(cell in groups[9] for cell in [1, 2, 6]):
        print("Error: Group 10 does not include cells 1, 2, and 6")
        return False
    
    # Calculate metrics
    surface_areas = []
    total_surface_area = 0
    for i in range(num_groups - 1):
        surface_area = sum(1 for u in groups[i] for v in groups[i+1] if graph.has_edge(u, v))
        surface_areas.append(surface_area)
        total_surface_area += surface_area
    
    print(f"Surface areas: {', '.join(map(str, surface_areas))}")
    print(f"Total surface area: {total_surface_area}")
    
    current_path_length = calculate_current_path_length(graph, groups)
    degradation_balance = calculate_degradation_balance(graph, groups)
    edge_cooling_score = calculate_edge_cooling_score(graph, groups)
    
    print(f"\nMetrics:")
    print(f"Current Path Length: {current_path_length:.2f}")
    print(f"Degradation Balance: {degradation_balance:.2f}")
    print(f"Edge Cooling Score: {edge_cooling_score}")
    print(f"Surface Area (Min/Max/StdDev): {min(surface_areas)}/{max(surface_areas)}/{stdev(surface_areas):.2f}")
    
    return True

def main():
    if len(sys.argv) < 2:
        print("Usage: python check.py <solution_json_file>")
        sys.exit(1)

    solution_file = sys.argv[1]
    print(f"Validating and analyzing solution from {solution_file}")
    
    original_data = load_json('paste.txt')
    solution_data = load_json(solution_file)
    
    if validate_and_analyze_solution(original_data, solution_data):
        print("\nSolution is valid and satisfies all constraints.")
    else:
        print("\nSolution is invalid or does not satisfy all constraints.")

if __name__ == "__main__":
    main()
