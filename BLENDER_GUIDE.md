# Blender Cyberpunk City Modeling Guide

This guide will help you create a cyberpunk city scene in Blender that can be exported and used in the Vulkan demo.

## Getting Started

1. **Download Blender**: Get the latest version from [blender.org](https://www.blender.org/download/)
2. **Open Blender**: Start with the default cube scene
3. **Set Units**: Go to Scene Properties → Units → Length → Meters

## Basic City Layout

### 1. Ground Plane
1. Delete the default cube (X → Delete)
2. Add a plane (Shift+A → Mesh → Plane)
3. Scale it up (S → 50 → Enter) - this will be our city ground
4. Name it "Ground" in the outliner

### 2. Create Building Blocks

#### Simple Building
1. Add a cube (Shift+A → Mesh → Cube)
2. Scale it to building proportions:
   - S → X → 2 → Enter (width)
   - S → Z → 2 → Enter (depth) 
   - S → Y → 8 → Enter (height)
3. Move it to position (G → X → 5 → Enter)
4. Name it "Building_01"

#### Duplicate and Vary Buildings
1. Select the building
2. Duplicate (Shift+D)
3. Move to new position (G → X → 15 → Enter)
4. Scale randomly (S → X → 1.5 → Enter, S → Y → 12 → Enter)
5. Repeat 10-15 times to create a city grid

### 3. Add Neon Elements

#### Neon Strips
1. Add a plane (Shift+A → Mesh → Plane)
2. Scale it thin (S → Y → 0.1 → Enter)
3. Scale it long (S → X → 3 → Enter)
4. Position it on a building face
5. Name it "Neon_Strip_01"

#### Neon Signs
1. Add a cube (Shift+A → Mesh → Cube)
2. Scale it flat (S → Y → 0.2 → Enter)
3. Scale it wide (S → X → 2 → Enter, S → Z → 0.5 → Enter)
4. Position it on building
5. Name it "Neon_Sign_01"

### 4. Create Materials

#### Building Material
1. Select a building
2. Go to Material Properties (red sphere icon)
3. Click "New"
4. Name it "Building_Material"
5. Set Base Color to dark blue-gray (0.1, 0.1, 0.2)
6. Set Metallic to 0.2
7. Set Roughness to 0.8

#### Neon Material
1. Select a neon element
2. Create new material "Neon_Material"
3. Set Base Color to bright cyan (0.0, 1.0, 1.0)
4. Set Emission to bright cyan (0.0, 1.0, 1.0)
5. Set Emission Strength to 2.0

#### Ground Material
1. Select the ground plane
2. Create new material "Ground_Material"
3. Set Base Color to dark gray (0.05, 0.05, 0.1)
4. Set Metallic to 0.1
5. Set Roughness to 0.9

### 5. Add Lighting

#### Main Light
1. Add a sun light (Shift+A → Light → Sun)
2. Position it high and angled (G → Z → 20 → Enter, R → X → -45 → Enter)
3. Set Energy to 3.0
4. Set Color to warm white (1.0, 0.9, 0.8)

#### Neon Lights
1. Add point lights near neon elements
2. Set Energy to 5.0
3. Set Color to match neon material
4. Set Radius to 10.0

### 6. Camera Setup

#### Main Camera
1. Select the default camera
2. Position it for good city view (G → X → 0 → Enter, G → Y → -20 → Enter, G → Z → 15 → Enter)
3. Rotate to look at city (R → X → 15 → Enter)
4. Set Focal Length to 50mm

#### Animation Camera (for car path)
1. Add another camera (Shift+A → Camera)
2. Position it at car height (G → Z → 2 → Enter)
3. Name it "Car_Camera"

## Export Settings

### 1. Prepare for Export
1. Select all objects (A)
2. Apply all transforms (Ctrl+A → All Transforms)
3. Make sure all materials are assigned

### 2. Export as glTF
1. File → Export → glTF 2.0 (.glb/.gltf)
2. Choose .glb format for single file
3. Export settings:
   - ✓ Include: Selected Objects
   - ✓ Include: Materials
   - ✓ Include: Textures
   - ✓ Include: Cameras
   - ✓ Include: Punctual Lights
   - ✓ Transform: +Y Up
   - ✓ Geometry: Apply Modifiers
   - ✓ Materials: Export

### 3. File Organization
- Save the .blend file as `cyberpunk_city.blend`
- Export as `cyberpunk_city.glb`
- Place in the `assets/` folder

## Pro Tips

### Building Variation
- Use different heights (3-20 units)
- Vary widths and depths
- Add small details like ledges or balconies
- Use different materials for variety

### Neon Placement
- Place neon strips on building edges
- Add neon signs on building faces
- Use different colors (cyan, magenta, yellow, green)
- Vary sizes and positions

### Lighting
- Use warm lights for contrast with cool neon
- Add some colored lights for atmosphere
- Use different light types (sun, point, spot)

### Performance
- Keep polygon count reasonable (under 100k triangles)
- Use instancing for repeated elements
- Optimize materials (avoid too many unique materials)

## Troubleshooting

### Common Issues
1. **Black materials**: Check if materials are assigned to objects
2. **Missing textures**: Make sure to pack textures in Blender
3. **Wrong scale**: Apply transforms before exporting
4. **Missing lights**: Enable light export in glTF settings

### Export Problems
1. **Large file size**: Use .glb format and optimize textures
2. **Missing materials**: Check material export settings
3. **Wrong orientation**: Set +Y Up in export settings

## Next Steps

After creating your city:
1. Export the scene as glTF
2. Test the export in a glTF viewer
3. Import into the Vulkan demo
4. Adjust materials and lighting as needed

Remember: Start simple and add complexity gradually. The demo needs to run at 60fps, so prioritize performance over detail.

