import os
import sys
import numpy as np
from PIL import Image, ImageOps, ImageDraw, ImageFilter

src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\40f7eec9-a25b-4c08-9237-9f0c34a9ccab\.user_uploaded\media_1786976020689.png"
root_dir = r"C:\Users\Atul\Desktop\Aerosync"

print("Loading uploaded logo from:", src_path)
raw_img = Image.open(src_path).convert("RGBA")
w, h = raw_img.size
print(f"Original image size: {w}x{h}")

# Analyze background color
arr = np.array(raw_img, dtype=np.float32)
corners = [arr[0:5, 0:5, :3], arr[0:5, -5:, :3], arr[-5:, 0:5, :3], arr[-5:, -5:, :3]]
bg_color = np.mean(corners, axis=(0, 1, 2))
print(f"Detected background RGB: {bg_color}")

# Distance from background color
dist = np.linalg.norm(arr[:, :, :3] - bg_color, axis=2)

# Calculate high-precision alpha mask
# Soft threshold: values near background become transparent, blue strokes stay fully opaque
alpha = np.clip((dist - 12.0) / 160.0, 0.0, 1.0)
alpha = np.power(alpha, 0.9)

# Unmultiply RGB so stroke colors on transparent background stay clean without white fringes
rgb_unmult = np.zeros_like(arr[:, :, :3])
for c in range(3):
    rgb_unmult[:, :, c] = np.clip(
        (arr[:, :, c] - bg_color[c] * (1.0 - alpha)) / np.maximum(alpha, 0.001),
        0.0, 255.0
    )

# Also enhance stroke saturation slightly for maximum visual vibrancy
rgba_raw = np.dstack([rgb_unmult, alpha * 255.0]).astype(np.uint8)
extracted_emblem = Image.fromarray(rgba_raw, "RGBA")

# Crop bounding box of non-transparent pixels
bbox = extracted_emblem.getbbox()
if bbox:
    cropped_emblem = extracted_emblem.crop(bbox)
else:
    cropped_emblem = extracted_emblem

print(f"Cropped emblem size: {cropped_emblem.size}")

# Master 1024x1024 transparent canvas
master_1024 = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
# Scale emblem to fit nicely centered with padding (e.g. 840x840 max bounding box)
ew, eh = cropped_emblem.size
scale = min(840.0 / ew, 840.0 / eh)
new_w, new_h = int(ew * scale), int(eh * scale)
scaled_emblem = cropped_emblem.resize((new_w, new_h), Image.Resampling.LANCZOS)
paste_pos = ((1024 - new_w) // 2, (1024 - new_h) // 2)
master_1024.paste(scaled_emblem, paste_pos, scaled_emblem)

# Master 512x512 transparent logo
logo_512 = master_1024.resize((512, 512), Image.Resampling.LANCZOS)

# Create a sleek dark-themed square launcher tile (#0B1329 with subtle neon border)
def create_launcher_tile(size, round_shape=False):
    tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tile)
    
    # Background color: Deep Navy #0F172A
    bg_col = (15, 23, 42, 255)
    
    if round_shape:
        draw.ellipse((0, 0, size, size), fill=bg_col)
    else:
        radius = int(size * 0.22)
        draw.rounded_rectangle((0, 0, size, size), radius=radius, fill=bg_col)
    
    # Place emblem in center (72% size)
    inner_size = int(size * 0.72)
    inner_logo = master_1024.resize((inner_size, inner_size), Image.Resampling.LANCZOS)
    pos = ((size - inner_size) // 2, (size - inner_size) // 2)
    tile.paste(inner_logo, pos, inner_logo)
    return tile

# -------------------------------------------------------------
# 1. Update Android Assets
# -------------------------------------------------------------
android_res = os.path.join(root_dir, "platform", "android", "app", "src", "main", "res")

# In-app transparent logo
drawable_dir = os.path.join(android_res, "drawable")
os.makedirs(drawable_dir, exist_ok=True)
android_logo_path = os.path.join(drawable_dir, "aerosync_logo.png")
logo_512.save(android_logo_path, "PNG")
print("Updated Android in-app logo:", android_logo_path)

# Android Adaptive Foreground (108dp canvas, 72dp emblem safe area = ~66%)
fg_adaptive = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
fg_size = int(512 * 0.68)
fg_inner = master_1024.resize((fg_size, fg_size), Image.Resampling.LANCZOS)
fg_pos = ((512 - fg_size) // 2, (512 - fg_size) // 2)
fg_adaptive.paste(fg_inner, fg_pos, fg_inner)
fg_path = os.path.join(drawable_dir, "ic_launcher_foreground.png")
fg_adaptive.save(fg_path, "PNG")
print("Updated Android adaptive icon foreground:", fg_path)

# Mipmaps for all densities
android_densities = {
    "mipmap-mdpi": 48,
    "mipmap-hdpi": 72,
    "mipmap-xhdpi": 96,
    "mipmap-xxhdpi": 144,
    "mipmap-xxxhdpi": 192,
}

for folder, size in android_densities.items():
    folder_path = os.path.join(android_res, folder)
    os.makedirs(folder_path, exist_ok=True)
    
    # Square / Rounded
    sq_tile = create_launcher_tile(size, round_shape=False)
    sq_path = os.path.join(folder_path, "ic_launcher.png")
    sq_tile.save(sq_path, "PNG")
    
    # Round
    round_tile = create_launcher_tile(size, round_shape=True)
    round_path = os.path.join(folder_path, "ic_launcher_round.png")
    round_tile.save(round_path, "PNG")
    print(f"Updated Android {folder} ({size}x{size})")

# -------------------------------------------------------------
# 2. Update Windows Assets (desktop_tauri and desktop_app)
# -------------------------------------------------------------
win_targets = [
    os.path.join(root_dir, "platform", "windows", "desktop_tauri"),
    os.path.join(root_dir, "platform", "windows", "desktop_app"),
]

for base in win_targets:
    assets_dir = os.path.join(base, "assets")
    os.makedirs(assets_dir, exist_ok=True)
    
    # In-app transparent logo
    win_logo_file = os.path.join(assets_dir, "logo.png")
    logo_512.save(win_logo_file, "PNG")
    print(f"Updated {win_logo_file}")
    
    # Multi-resolution ICO
    win_ico_file = os.path.join(assets_dir, "icon.ico")
    ico_master = create_launcher_tile(256, round_shape=False)
    ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
    ico_master.save(win_ico_file, format="ICO", sizes=ico_sizes)
    print(f"Updated {win_ico_file}")
    
    # Tauri app icons
    tauri_icons_dir = os.path.join(base, "src-tauri", "icons")
    os.makedirs(tauri_icons_dir, exist_ok=True)
    
    # icon.ico
    tauri_ico_path = os.path.join(tauri_icons_dir, "icon.ico")
    ico_master.save(tauri_ico_path, format="ICO", sizes=ico_sizes)
    
    # icon.png (512x512)
    create_launcher_tile(512, round_shape=False).save(os.path.join(tauri_icons_dir, "icon.png"), "PNG")
    
    # 128x128.png
    create_launcher_tile(128, round_shape=False).save(os.path.join(tauri_icons_dir, "128x128.png"), "PNG")
    
    # 128x128@2x.png (256x256)
    create_launcher_tile(256, round_shape=False).save(os.path.join(tauri_icons_dir, "128x128@2x.png"), "PNG")
    
    # 32x32.png
    create_launcher_tile(32, round_shape=False).save(os.path.join(tauri_icons_dir, "32x32.png"), "PNG")
    print(f"Updated Tauri icons in {tauri_icons_dir}")

print("\nSUCCESS: All AeroSync logos and app icons updated completely across Windows and Android!")
