import matplotlib.pyplot as plt
import matplotlib.patches as patches
from matplotlib.widgets import Slider, TextBox, CheckButtons
import numpy as np
from collections import defaultdict

class Cell:
    def __init__(self, x, y, orientation, generation, parent_lineage_id=0, unique_id=0, child_number=0):
        self.x = x
        self.y = y
        self.orientation = orientation  # in degrees, 0 = right, counter-clockwise
        self.generation = generation
        self.parent_lineage_id = parent_lineage_id  # AA: Parent's unique ID
        self.unique_id = unique_id  # BB: This cell's unique ID
        self.child_number = child_number  # C: Child number (1 or 2, 0 for root)
        self.children = []
        
    def get_lineage_string(self):
        """Generate lineage string in A.B.C format"""
        return f"{self.parent_lineage_id}.{self.unique_id}.{self.child_number}"
        
    def __repr__(self):
        return f"Cell({self.get_lineage_string()}, pos=({self.x},{self.y}), ori={self.orientation:.1f}°)"

class TissueGrowthSimulator:
    def __init__(self):
        # Genome parameters (global, applies to all cells)
        self.split_angle = 0.0  # degrees
        self.child1_angle = 0.0  # degrees
        self.child2_angle = 0.0  # degrees
        self.make_adhesion = True
        self.keep_adhesion_child1 = True
        self.keep_adhesion_child2 = True
        
        # Grid parameters
        self.grid_spacing = 1.0  # spacing between grid cells (1 unit)
        self.cell_radius = 0.4  # visual radius for drawing
        
        # Simulation state
        self.max_generation = 0
        self.current_generation = 0
        self.all_cells = []
        self.active_cells = []  # Only cells that haven't split yet
        self.adhesions = []
        self.next_unique_id = 1  # Counter for unique IDs
        
        # Grid occupancy tracking
        self.grid = {}  # (grid_x, grid_y) -> Cell
        
        # Dragging state
        self.dragging_cell = None
        self.drag_offset_x = 0
        self.drag_offset_y = 0
        
        # Initialize with generation 0
        self.reset_simulation()
        
        # Setup matplotlib
        self.setup_plot()
        
    def reset_simulation(self):
        """Reset to generation 0 with a single cell"""
        self.all_cells = []
        self.active_cells = []
        self.adhesions = []
        self.grid = {}
        self.next_unique_id = 1
        
        # Create initial cell at origin (lineage: 0.1.0)
        initial_cell = Cell(0, 0, 0.0, 0, parent_lineage_id=0, unique_id=1, child_number=0)
        self.all_cells.append(initial_cell)
        self.active_cells.append(initial_cell)
        self.grid[(0, 0)] = initial_cell
        self.next_unique_id = 2
        
    def world_to_grid(self, x, y):
        """Convert world coordinates to grid coordinates"""
        return (round(x / self.grid_spacing), round(y / self.grid_spacing))
    
    def grid_to_world(self, grid_x, grid_y):
        """Convert grid coordinates to world coordinates"""
        return (grid_x * self.grid_spacing, grid_y * self.grid_spacing)
    
    def push_cell(self, grid_pos, direction):
        """Push a cell at grid_pos in the given direction, recursively pushing others"""
        if grid_pos not in self.grid:
            return grid_pos  # Position is free
        
        cell_to_push = self.grid[grid_pos]
        
        # Calculate new position
        dx, dy = direction
        new_grid_pos = (grid_pos[0] + dx, grid_pos[1] + dy)
        
        # Recursively push any cell at the new position
        self.push_cell(new_grid_pos, direction)
        
        # Move the cell
        del self.grid[grid_pos]
        self.grid[new_grid_pos] = cell_to_push
        cell_to_push.x, cell_to_push.y = self.grid_to_world(*new_grid_pos)
        
        return grid_pos  # Original position is now free
    
    def place_cell_in_grid(self, parent_cell, offset_angle):
        """Place a child cell adjacent to parent, pushing others if needed"""
        # Calculate direction perpendicular to split angle
        angle_rad = np.radians(parent_cell.orientation + self.split_angle + offset_angle)
        dx = np.cos(angle_rad)
        dy = np.sin(angle_rad)
        
        # Determine grid direction (quantize to nearest grid axis)
        grid_dx = 1 if dx > 0.5 else (-1 if dx < -0.5 else 0)
        grid_dy = 1 if dy > 0.5 else (-1 if dy < -0.5 else 0)
        
        # If both are zero, default to right
        if grid_dx == 0 and grid_dy == 0:
            grid_dx = 1
        
        parent_grid = self.world_to_grid(parent_cell.x, parent_cell.y)
        target_grid = (parent_grid[0] + grid_dx, parent_grid[1] + grid_dy)
        
        # Push any existing cell
        if target_grid in self.grid:
            self.push_cell(target_grid, (grid_dx, grid_dy))
        
        # Place new cell
        world_x, world_y = self.grid_to_world(*target_grid)
        return world_x, world_y
    
    def split_cell(self, cell):
        """Split a cell into two children with 1 unit spacing"""
        # Calculate child orientations
        child1_orientation = (cell.orientation + self.child1_angle) % 360
        child2_orientation = (cell.orientation + self.child2_angle) % 360
        
        # Get parent's grid position
        parent_grid = self.world_to_grid(cell.x, cell.y)
        
        # Calculate split direction (along the split angle axis, perpendicular to zone C)
        # Zone C is the equatorial plane, cells split along the polar axis
        split_axis_rad = np.radians(cell.orientation + self.split_angle)
        dx = np.cos(split_axis_rad)
        dy = np.sin(split_axis_rad)
        
        # Determine grid direction for splitting (quantize to nearest axis)
        grid_dx = 1 if dx > 0.5 else (-1 if dx < -0.5 else 0)
        grid_dy = 1 if dy > 0.5 else (-1 if dy < -0.5 else 0)
        
        # If both are zero, default to vertical split
        if grid_dx == 0 and grid_dy == 0:
            grid_dy = 1
        
        # REMOVE parent from grid first
        if parent_grid in self.grid and self.grid[parent_grid] == cell:
            del self.grid[parent_grid]
        if cell in self.active_cells:
            self.active_cells.remove(cell)
        
        # For 1 unit spacing between children, they should be at parent position and parent+1
        # Child1 stays at parent position, child2 goes 1 unit away
        child1_grid = parent_grid
        child2_grid = (parent_grid[0] + grid_dx, parent_grid[1] + grid_dy)
        
        # Push any cells in the way of child2
        if child2_grid in self.grid:
            self.push_cell(child2_grid, (grid_dx, grid_dy))
        
        # Place children at their grid positions
        child1_x, child1_y = self.grid_to_world(*child1_grid)
        child2_x, child2_y = self.grid_to_world(*child2_grid)
        
        # Create children with lineage IDs
        child1 = Cell(child1_x, child1_y, child1_orientation, cell.generation + 1,
                     parent_lineage_id=cell.unique_id, unique_id=self.next_unique_id, child_number=1)
        self.next_unique_id += 1
        
        child2 = Cell(child2_x, child2_y, child2_orientation, cell.generation + 1,
                     parent_lineage_id=cell.unique_id, unique_id=self.next_unique_id, child_number=2)
        self.next_unique_id += 1
        
        cell.children = [child1, child2]
        
        # Add children to grid
        self.grid[child1_grid] = child1
        self.grid[child2_grid] = child2
        
        # Adhesion inheritance: transfer parent's bonds to children
        # Process each adhesion involving the parent
        for adhesion in self.adhesions[:]:  # Copy to avoid modification during iteration
            # Unpack adhesion (handle multiple formats)
            source_child_num = None
            if len(adhesion) == 4:
                cell1_in_adhesion, cell2_in_adhesion, is_inherited, source_child_num = adhesion
            elif len(adhesion) == 3:
                cell1_in_adhesion, cell2_in_adhesion, is_inherited = adhesion
            else:
                cell1_in_adhesion, cell2_in_adhesion = adhesion
                is_inherited = False
            
            # Check if parent is in this adhesion
            neighbor = None
            neighbor_source_child = None
            if cell1_in_adhesion == cell:
                neighbor = cell2_in_adhesion
                neighbor_source_child = source_child_num  # Which child type from neighbor
                self.adhesions.remove(adhesion)  # Remove parent bond
            elif cell2_in_adhesion == cell:
                neighbor = cell1_in_adhesion
                neighbor_source_child = source_child_num  # Which child type from neighbor
                self.adhesions.remove(adhesion)  # Remove parent bond
            else:
                continue  # Parent not in this adhesion
            
            # Calculate bond geometry
            dx = neighbor.x - cell.x
            dy = neighbor.y - cell.y
            if dx == 0 and dy == 0:
                continue  # Skip zero-length bonds
            
            # Normalize bond direction
            bond_length = np.sqrt(dx**2 + dy**2)
            bond_dir = np.array([dx / bond_length, dy / bond_length])
            
            # Split direction
            split_rad = np.radians(cell.orientation + self.split_angle)
            split_dir = np.array([np.cos(split_rad), np.sin(split_rad)])
            
            # Calculate angle between bond and split direction
            dot_product = np.dot(bond_dir, split_dir)
            angle_deg = np.degrees(np.arccos(np.clip(dot_product, -1.0, 1.0)))
            
            print(f"\n=== Inheritance: {cell.get_lineage_string()} -> {neighbor.get_lineage_string()} ===")
            print(f"Cell position: ({cell.x:.1f}, {cell.y:.1f})")
            print(f"Neighbor position: ({neighbor.x:.1f}, {neighbor.y:.1f})")
            print(f"Bond direction: ({dx:.2f}, {dy:.2f})")
            print(f"Split direction: ({split_dir[0]:.2f}, {split_dir[1]:.2f})")
            print(f"Bond angle from split: {angle_deg:.1f}°")
            print(f"Neighbor has children: {len(neighbor.children) == 2}")
            print(f"Source child num from bond: {neighbor_source_child}")
            
            # Determine which children inherit based on angle
            # Zone thresholds
            equatorial_threshold = 10.0  # Within 10° of perpendicular (90°)
            
            # PRIORITY: If we have source_child_num metadata, use it to match child types
            if neighbor_source_child is not None:
                print(f"Using metadata: source_child_num={neighbor_source_child}")
                if neighbor_source_child == 1 and self.keep_adhesion_child1:
                    self.adhesions.append((child1, neighbor, True))
                    print(f"  CREATING BOND: {child1.get_lineage_string()} -> {neighbor.get_lineage_string()} (matched by metadata)")
                elif neighbor_source_child == 2 and self.keep_adhesion_child2:
                    self.adhesions.append((child2, neighbor, True))
                    print(f"  CREATING BOND: {child2.get_lineage_string()} -> {neighbor.get_lineage_string()} (matched by metadata)")
            elif abs(angle_deg - 90.0) <= equatorial_threshold:
                # Zone C: Perpendicular - children connect to SAME child type
                print(f"Zone C (perpendicular) - same child type connections")
                print(f"  neighbor_source_child = {neighbor_source_child}")
                
                # Check if we know which child type the neighbor bond came from
                if neighbor_source_child is not None:
                    # We have metadata - match to the specified child type
                    if neighbor_source_child == 1 and self.keep_adhesion_child1:
                        self.adhesions.append((child1, neighbor, True))
                        print(f"  CREATING BOND: {child1.get_lineage_string()} -> {neighbor.get_lineage_string()} (matched by source_child_num=1)")
                    elif neighbor_source_child == 2 and self.keep_adhesion_child2:
                        self.adhesions.append((child2, neighbor, True))
                        print(f"  CREATING BOND: {child2.get_lineage_string()} -> {neighbor.get_lineage_string()} (matched by source_child_num=2)")
                    else:
                        print(f"  NO BOND CREATED (source_child_num={neighbor_source_child} didn't match or keep_adhesion disabled)")
                elif len(neighbor.children) == 2:
                    # Neighbor has split - connect same child types
                    neighbor_child1, neighbor_child2 = neighbor.children
                    if self.keep_adhesion_child1:
                        self.adhesions.append((child1, neighbor_child1, True))
                        print(f"  child1 -> neighbor.child1")
                    if self.keep_adhesion_child2:
                        self.adhesions.append((child2, neighbor_child2, True))
                        print(f"  child2 -> neighbor.child2")
                else:
                    # Neighbor hasn't split yet - store child type metadata
                    if self.keep_adhesion_child1:
                        self.adhesions.append((child1, neighbor, True, 1))
                        print(f"  child1 -> neighbor (will match to neighbor's child1)")
                    if self.keep_adhesion_child2:
                        self.adhesions.append((child2, neighbor, True, 2))
                        print(f"  child2 -> neighbor (will match to neighbor's child2)")
            elif angle_deg < 90.0:
                # Zone B: Aligned with split - child2 inherits
                print(f"Zone B (aligned) - child2 inherits")
                if self.keep_adhesion_child2:
                    if len(neighbor.children) == 2:
                        # Connect to appropriate neighbor child based on geometry
                        self.adhesions.append((child2, neighbor.children[1], True))
                    else:
                        self.adhesions.append((child2, neighbor, True))
            else:
                # Zone A: Opposite to split - child1 inherits
                print(f"Zone A (opposite) - child1 inherits")
                if self.keep_adhesion_child1:
                    if len(neighbor.children) == 2:
                        # Connect to appropriate neighbor child based on geometry
                        self.adhesions.append((child1, neighbor.children[0], True))
                    else:
                        self.adhesions.append((child1, neighbor, True))
        
        # Add adhesion between siblings if enabled
        # Mark sibling adhesions with a flag (tuple with 3 elements: cell1, cell2, is_inherited)
        if self.make_adhesion:
            self.adhesions.append((child1, child2, False))  # False = not inherited
        
        return child1, child2
    
    def simulate_to_generation(self, target_gen):
        """Simulate from generation 0 to target generation"""
        self.reset_simulation()
        
        for gen in range(target_gen):
            # Get all active cells at current generation
            cells_to_split = [c for c in self.active_cells if c.generation == gen]
            
            # Split each cell
            for cell in cells_to_split:
                child1, child2 = self.split_cell(cell)
                self.all_cells.append(child1)
                self.all_cells.append(child2)
                self.active_cells.append(child1)
                self.active_cells.append(child2)
        
        self.current_generation = target_gen
    
    def draw_cell(self, ax, cell):
        """Draw a cell with colored hemispheres and equatorial zone"""
        x, y = cell.x, cell.y
        r = self.cell_radius
        
        # Calculate angles for zones
        # split_angle defines the orientation of the cell's axis (where poles point)
        split_rad = np.radians(cell.orientation + self.split_angle)
        split_deg = np.degrees(split_rad)
        
        # The equatorial zone C is perpendicular to the split_angle axis
        # So zone C is centered at split_angle + 90° and split_angle - 90°
        equator_angle = split_deg + 90
        
        # Draw blue hemisphere (one pole)
        # Blue pole is centered at split_angle direction
        wedge_blue = patches.Wedge((x, y), r, split_deg - 90, split_deg + 90, 
                                    facecolor='blue', edgecolor='none')
        ax.add_patch(wedge_blue)
        
        # Draw green hemisphere (opposite pole)
        # Green pole is centered at split_angle + 180° direction
        wedge_green = patches.Wedge((x, y), r, split_deg + 90, split_deg + 270, 
                                     facecolor='green', edgecolor='none')
        ax.add_patch(wedge_green)
        
        # Draw red equatorial zone C (10° band perpendicular to split_angle)
        # Zone C runs around the equator, perpendicular to the pole axis
        eq1_start = equator_angle - 5
        eq1_end = equator_angle + 5
        wedge_eq1 = patches.Wedge((x, y), r, eq1_start, eq1_end, 
                                  facecolor='red', edgecolor='none')
        ax.add_patch(wedge_eq1)
        
        # Second part of equatorial zone on opposite side
        eq2_start = equator_angle + 180 - 5
        eq2_end = equator_angle + 180 + 5
        wedge_eq2 = patches.Wedge((x, y), r, eq2_start, eq2_end, 
                                  facecolor='red', edgecolor='none')
        ax.add_patch(wedge_eq2)
        
        # Draw black outline for the cell
        circle_outline = patches.Circle((x, y), r, facecolor='none', 
                                       edgecolor='black', linewidth=1)
        ax.add_patch(circle_outline)
        
        # Draw orientation arrow (white, fits within cell radius)
        arrow_length = r * 0.6  # Fits within cell radius
        arrow_dx = arrow_length * np.cos(np.radians(cell.orientation))
        arrow_dy = arrow_length * np.sin(np.radians(cell.orientation))
        ax.arrow(x, y, arrow_dx, arrow_dy, 
                head_width=0.15, head_length=0.1, fc='white', ec='white', linewidth=1.5)
        
        # Draw lineage ID with dynamic positioning
        # Find a good position for the label that doesn't overlap with other cells
        label_offset = self.find_label_position(cell)
        label_x = x + label_offset[0]
        label_y = y + label_offset[1]
        
        # Draw line from label to cell
        ax.plot([label_x, x], [label_y, y], 'k-', linewidth=0.5, alpha=0.3, zorder=1)
        
        # Draw label with background
        ax.text(label_x, label_y, cell.get_lineage_string(), 
               ha='center', va='center', fontsize=7,
               bbox=dict(boxstyle='round,pad=0.3', facecolor='white', edgecolor='gray', alpha=0.8))
    
    def find_label_position(self, cell):
        """Find a good position for the cell's label that avoids overlapping with other cells"""
        # Get all visible cells
        visible_cells = [c for c in self.active_cells if c.generation <= self.current_generation]
        
        # Try different angles around the cell to find the best position
        label_distance = 1.5  # Distance from cell center to label (increased)
        best_angle = -90  # Default: below the cell
        best_score = -float('inf')
        
        # Test 8 directions around the cell
        for angle_deg in [0, 45, 90, 135, 180, 225, 270, 315]:
            angle_rad = np.radians(angle_deg)
            test_x = label_distance * np.cos(angle_rad)
            test_y = label_distance * np.sin(angle_rad)
            
            # Calculate score based on distance to other cells (higher is better)
            score = 0
            min_safe_distance = self.cell_radius + 0.5  # Minimum safe distance from cells
            
            for other_cell in visible_cells:
                if other_cell == cell:
                    continue
                dist = np.sqrt((cell.x + test_x - other_cell.x)**2 + 
                              (cell.y + test_y - other_cell.y)**2)
                
                # Heavily penalize positions too close to other cells
                if dist < min_safe_distance:
                    score -= 100  # Large penalty
                else:
                    score += dist  # Prefer positions far from other cells
            
            if score > best_score:
                best_score = score
                best_angle = angle_deg
        
        # Return offset for the best position
        angle_rad = np.radians(best_angle)
        return (label_distance * np.cos(angle_rad), label_distance * np.sin(angle_rad))
    
    def get_adhesion_anchor_point(self, cell, other_cell):
        """Get anchor point for adhesion on the edge of the cell facing the other cell"""
        # Calculate direction from cell to other cell
        dx = other_cell.x - cell.x
        dy = other_cell.y - cell.y
        distance = np.sqrt(dx**2 + dy**2)
        
        if distance > 0:
            # Normalize direction
            dir_x = dx / distance
            dir_y = dy / distance
            
            # Place anchor on the edge of the cell in the direction of the other cell
            anchor_x = cell.x + dir_x * self.cell_radius
            anchor_y = cell.y + dir_y * self.cell_radius
            return anchor_x, anchor_y
        
        # Fallback if cells are at same position (shouldn't happen)
        return cell.x, cell.y
    
    def draw_adhesion(self, ax, cell1, cell2, is_inherited=False):
        """Draw adhesion line between two cells with anchor points"""
        # Get anchor points - they point toward each other
        anchor1_x, anchor1_y = self.get_adhesion_anchor_point(cell1, cell2)
        anchor2_x, anchor2_y = self.get_adhesion_anchor_point(cell2, cell1)
        
        # Draw yellow dots at anchor points (behind line)
        ax.plot(anchor1_x, anchor1_y, 'o', color='yellow', markersize=4, zorder=5)
        ax.plot(anchor2_x, anchor2_y, 'o', color='yellow', markersize=4, zorder=5)
        
        # Draw adhesion line between anchor points (on top of dots)
        ax.plot([anchor1_x, anchor2_x], [anchor1_y, anchor2_y], 
               color='orange', linewidth=2, alpha=0.8, zorder=10)
    
    def update_plot(self):
        """Redraw the entire plot"""
        self.ax.clear()
        
        # Get only active cells (cells that haven't been removed by splitting)
        visible_cells = [c for c in self.active_cells if c.generation <= self.current_generation]
        
        # Draw adhesions first (so they're behind cells)
        for adhesion in self.adhesions:
            # Unpack adhesion (handle both old 2-tuple and new 3-tuple format)
            if len(adhesion) == 3:
                cell1, cell2, is_inherited = adhesion
            else:
                cell1, cell2 = adhesion
                is_inherited = False
                
            if cell1 in visible_cells and cell2 in visible_cells:
                self.draw_adhesion(self.ax, cell1, cell2, is_inherited)
        
        # Draw cells
        for cell in visible_cells:
            self.draw_cell(self.ax, cell)
        
        # Set axis properties
        if visible_cells:
            xs = [c.x for c in visible_cells]
            ys = [c.y for c in visible_cells]
            margin = 5
            self.ax.set_xlim(min(xs) - margin, max(xs) + margin)
            self.ax.set_ylim(min(ys) - margin, max(ys) + margin)
        else:
            self.ax.set_xlim(-5, 5)
            self.ax.set_ylim(-5, 5)
        
        self.ax.set_aspect('equal')
        self.ax.grid(False)
        self.ax.set_title(f'Tissue Growth Simulator - Generation {self.current_generation}')
        
        self.fig.canvas.draw_idle()
    
    def on_generation_change(self, val):
        """Handle generation slider change"""
        target_gen = int(val)
        if target_gen != self.current_generation:
            self.simulate_to_generation(target_gen)
            self.update_plot()
    
    def on_param_change(self, param_name):
        """Handle parameter text box change"""
        def handler(text):
            try:
                value = float(text)
                setattr(self, param_name, value)
                # Re-simulate with new parameters
                self.simulate_to_generation(self.current_generation)
                self.update_plot()
            except ValueError:
                pass
        return handler
    
    def on_make_adhesion_change(self, label):
        """Handle make_adhesion checkbox change"""
        self.make_adhesion = not self.make_adhesion
        self.simulate_to_generation(self.current_generation)
        self.update_plot()
    
    def on_keep_adhesion_child1_change(self, label):
        """Handle keep_adhesion_child1 checkbox change"""
        self.keep_adhesion_child1 = not self.keep_adhesion_child1
        self.simulate_to_generation(self.current_generation)
        self.update_plot()
    
    def on_keep_adhesion_child2_change(self, label):
        """Handle keep_adhesion_child2 checkbox change"""
        self.keep_adhesion_child2 = not self.keep_adhesion_child2
        self.simulate_to_generation(self.current_generation)
        self.update_plot()
    
    def on_mouse_press(self, event):
        """Handle mouse press for cell dragging"""
        if event.inaxes != self.ax:
            return
        
        # Find cell at click position
        visible_cells = [c for c in self.active_cells if c.generation <= self.current_generation]
        for cell in visible_cells:
            dist = np.sqrt((cell.x - event.xdata)**2 + (cell.y - event.ydata)**2)
            if dist <= self.cell_radius:
                self.dragging_cell = cell
                self.drag_offset_x = event.xdata - cell.x
                self.drag_offset_y = event.ydata - cell.y
                break
    
    def on_mouse_release(self, event):
        """Handle mouse release to stop dragging"""
        self.dragging_cell = None
    
    def on_mouse_move(self, event):
        """Handle mouse move for cell dragging"""
        if self.dragging_cell is None or event.inaxes != self.ax:
            return
        
        # Update cell position
        self.dragging_cell.x = event.xdata - self.drag_offset_x
        self.dragging_cell.y = event.ydata - self.drag_offset_y
        
        # Redraw
        self.update_plot()
    
    def setup_plot(self):
        """Setup matplotlib figure and widgets"""
        self.fig = plt.figure(figsize=(14, 10))
        
        # Main plot area
        self.ax = plt.axes([0.1, 0.28, 0.8, 0.62])
        
        # Enable zoom and pan
        self.ax.set_navigate(True)
        self.fig.canvas.toolbar_visible = True
        
        # Generation slider
        ax_gen = plt.axes([0.1, 0.18, 0.8, 0.03])
        self.slider_gen = Slider(ax_gen, 'Generation Depth', 0, 10, valinit=0, valstep=1)
        self.slider_gen.on_changed(self.on_generation_change)
        
        # Angle parameters (row 1)
        param_y1 = 0.10
        param_width = 0.15
        param_spacing = 0.22
        
        # Split angle
        ax_split = plt.axes([0.1, param_y1, param_width, 0.04])
        self.text_split = TextBox(ax_split, 'Split Angle (°)', initial=str(self.split_angle))
        self.text_split.on_submit(self.on_param_change('split_angle'))
        
        # Child 1 angle
        ax_child1 = plt.axes([0.1 + param_spacing, param_y1, param_width, 0.04])
        self.text_child1 = TextBox(ax_child1, 'Child 1 Angle (°)', initial=str(self.child1_angle))
        self.text_child1.on_submit(self.on_param_change('child1_angle'))
        
        # Child 2 angle
        ax_child2 = plt.axes([0.1 + param_spacing * 2, param_y1, param_width, 0.04])
        self.text_child2 = TextBox(ax_child2, 'Child 2 Angle (°)', initial=str(self.child2_angle))
        self.text_child2.on_submit(self.on_param_change('child2_angle'))
        
        # Checkboxes (row 2)
        param_y2 = 0.02
        checkbox_width = 0.18
        checkbox_spacing = 0.20
        
        # Make adhesion checkbox
        ax_make_adhesion = plt.axes([0.1, param_y2, checkbox_width, 0.06])
        self.check_make_adhesion = CheckButtons(ax_make_adhesion, ['Make Adhesion'], [self.make_adhesion])
        self.check_make_adhesion.on_clicked(self.on_make_adhesion_change)
        
        # Keep adhesion child 1 checkbox
        ax_keep_adhesion_child1 = plt.axes([0.1 + checkbox_spacing, param_y2, checkbox_width, 0.06])
        self.check_keep_adhesion_child1 = CheckButtons(ax_keep_adhesion_child1, ['Keep Adhesion Child 1'], [self.keep_adhesion_child1])
        self.check_keep_adhesion_child1.on_clicked(self.on_keep_adhesion_child1_change)
        
        # Keep adhesion child 2 checkbox
        ax_keep_adhesion_child2 = plt.axes([0.1 + checkbox_spacing * 2, param_y2, checkbox_width, 0.06])
        self.check_keep_adhesion_child2 = CheckButtons(ax_keep_adhesion_child2, ['Keep Adhesion Child 2'], [self.keep_adhesion_child2])
        self.check_keep_adhesion_child2.on_clicked(self.on_keep_adhesion_child2_change)
        
        # Connect mouse events for cell dragging
        self.fig.canvas.mpl_connect('button_press_event', self.on_mouse_press)
        self.fig.canvas.mpl_connect('button_release_event', self.on_mouse_release)
        self.fig.canvas.mpl_connect('motion_notify_event', self.on_mouse_move)
        
        # Initial draw
        self.update_plot()
    
    def run(self):
        """Start the interactive simulation"""
        plt.show()

if __name__ == "__main__":
    simulator = TissueGrowthSimulator()
    simulator.run()
