import json
import sys

def load_json(filename):
    with open(filename, 'r') as file:
        return json.load(file)

def generate_svg(cells_data, solution_data, output_file):
    cell_diameter = 21.0  # mm
    cell_radius = cell_diameter / 2
    nestling_factor = 0.134  # This value can be adjusted if needed

    x_coords = [cell['x'] for cell in cells_data]
    y_coords = [cell['y'] for cell in cells_data]
    x_min, x_max = min(x_coords), max(x_coords)
    y_min, y_max = min(y_coords), max(y_coords)

    width = (x_max - x_min) * cell_radius + cell_diameter
    height = (y_max - y_min) * (cell_radius - 8 * nestling_factor) + cell_diameter

    svg_content = f'''<?xml version="1.0" encoding="UTF-8" standalone="no"?>
<svg width="{width}mm" height="{height}mm" viewBox="0 0 {width} {height}" xmlns="http://www.w3.org/2000/svg">
<title>Battery Cell Arrangement (1:1 scale)</title>
<g>
'''

    colors = ['#1f77b4', '#ff7f0e', '#2ca02c', '#d62728', '#9467bd', '#8c564b', '#e377c2', '#7f7f7f', '#bcbd22', '#17becf']

    for cell in cells_data:
        x = (cell['x'] - x_min) * cell_radius + cell_radius
        y = (cell['y'] - y_min) * (cell_radius - 8 * nestling_factor) + cell_radius

        group = solution_data.get(str(cell['n']))
        fill_color = colors[int(group) - 1] if group else 'none'
        fill_opacity = '0.3' if group else '0'

        svg_content += f'''  <circle cx="{x}" cy="{y}" r="{cell_radius}" fill="{fill_color}" fill-opacity="{fill_opacity}" stroke="black" stroke-width="0.5"/>
  <text x="{x}" y="{y}" font-size="4" text-anchor="middle" dy=".3em">{cell['n']}</text>
'''

    svg_content += f'''</g>
<text x="{width/2}" y="{height-5}" font-size="8" text-anchor="middle">{width:.1f} mm</text>
<text x="5" y="{height/2}" font-size="8" text-anchor="middle" transform="rotate(-90,5,{height/2})">{height:.1f} mm</text>
</svg>'''

    with open(output_file, 'w') as f:
        f.write(svg_content)

    print(f"SVG saved as {output_file}")
    print(f"Image dimensions: {width:.1f} mm x {height:.1f} mm")

def main():
    if len(sys.argv) < 2:
        print("Usage: python generate_battery_svg.py <cells_data_file> [solution_file]")
        sys.exit(1)

    cells_data_file = sys.argv[1]
    cells_data = load_json(cells_data_file)

    if len(sys.argv) >= 3:
        solution_file = sys.argv[2]
        solution_data = load_json(solution_file)
        output_file = 'battery_arrangement_grouped_correct_nestling.svg'
    else:
        solution_data = {}
        output_file = 'battery_arrangement_correct_nestling.svg'

    generate_svg(cells_data, solution_data, output_file)

if __name__ == "__main__":
    main()
