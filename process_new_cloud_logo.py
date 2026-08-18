import os
import sys
import numpy as np
from PIL import Image, ImageOps, ImageDraw, ImageFilter

src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\1972648b-0a85-418d-9ea7-e295e9dd6fb3\.user_uploaded\media_1787029635197.png"

print("Loading user uploaded cloud logo from:", src_path)
img = Image.open(src_path).convert("RGBA")
width, height = img.size
arr = np.array(img, dtype=np.float32)

# Background color estimation from corners
bg_r = np.mean([arr[0:5, 0:5, 0], arr[0:5, -5:, 0], arr[-5:, 0:5, 0], arr[-5:, -5:, 0]])
bg_g = np.mean([arr[0:5, 0:5, 1], arr[0:5, -5:, 1], arr[-5:, 0:5, 1], arr[-5:, -5:, 1]])
bg_b = np.mean([arr[0:5, 0:5, 2], arr[0:5, -5:, 2], arr[-5:, 0:5, 2], arr[-5:, -5:, 2]])
print(f"Estimated BG RGB: ({bg_r:.1f}, {bg_g:.1f}, {bg_b:.1f})")

# Calculate color distance and cyan intensity
# Cyan is high in G and B, low in R
cyan_intensity = (arr[:, :, 1] - bg_g) * 0.4 + (arr[:, :, 2] - bg_b) * 0.6
cyan_intensity = np.maximum(cyan_intensity, 0.0)

# Max cyan value
max_cyan = np.max(cyan_intensity)
alpha = cyan_intensity / (max_cyan * 0.75)
alpha = np.clip(alpha, 0.0, 1.0)

# Soft curve for anti-aliasing
alpha = np.where(alpha > 0.1, (alpha - 0.1) / 0.9, 0.0)
alpha = np.power(alpha, 0.9)
alpha = np.clip(alpha * 255.0, 0, 255).astype(np.uint8)

# Construct transparent emblem image
emblem_arr = np.zeros((height, width, 4), dtype=np.uint8)
# Bright cyan color: RGB(0, 210, 255) to RGB(10, 225, 255)
emblem_arr[:, :, 0] = np.clip(arr[:, :, 0] * 1.2, 0, 30).astype(np.uint8)
emblem_arr[:, :, 1] = np.clip(arr[:, :, 1] * 1.15, 0, 235).astype(np.uint8)
emblem_arr[:, :, 2] = np.clip(arr[:, :, 2] * 1.1, 0, 255).astype(np.uint8)
emblem_arr[:, :, 3] = alpha

raw_emblem = Image.fromarray(emblem_arr, mode='RGBA')

# Crop to tight bounding box of non-zero alpha
bbox = raw_emblem.getbbox()
if bbox:
    cropped = raw_emblem.crop(bbox)
else:
    cropped = raw_emblem

# Create a clean high-res 1024x1024 master transparent logo
master_1024 = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
crop_w, crop_h = cropped.size
scale = min(820 / crop_w, 820 / crop_h)
new_w = int(crop_w * scale)
new_h = int(crop_h * scale)
resized_crop = cropped.resize((new_w, new_h), Image.Resampling.LANCZOS)
paste_pos = ((1024 - new_w) // 2, (1024 - new_h) // 2)
master_1024.paste(resized_crop, paste_pos, resized_crop)

logo_512 = master_1024.resize((512, 512), Image.Resampling.LANCZOS)
logo_256 = master_1024.resize((256, 256), Image.Resampling.LANCZOS)
logo_128 = master_1024.resize((128, 128), Image.Resampling.LANCZOS)
logo_32 = master_1024.resize((32, 32), Image.Resampling.LANCZOS)

# Create Dark Navy App Tile (for Windows desktop app icon & Android launcher icon)
tile_512 = Image.new("RGBA", (512, 512), (10, 14, 26, 255))
inner_size = int(512 * 0.76)
inner_logo = master_1024.resize((inner_size, inner_size), Image.Resampling.LANCZOS)
tile_512.paste(inner_logo, ((512 - inner_size) // 2, (512 - inner_size) // 2), inner_logo)

# Multi-resolution ICO
ico_tile = Image.new("RGBA", (256, 256), (10, 14, 26, 255))
ico_inner_size = int(256 * 0.78)
ico_inner = master_1024.resize((ico_inner_size, ico_inner_size), Image.Resampling.LANCZOS)
ico_tile.paste(ico_inner, ((256 - ico_inner_size) // 2, ((256 - ico_inner_size) // 2)), ico_inner)

ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]

# Output targets
targets_tauri = [
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_tauri",
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_app"
]

for base in targets_tauri:
    # 1. src-tauri/icons
    icons_dir = os.path.join(base, "src-tauri", "icons")
    os.makedirs(icons_dir, exist_ok=True)
    ico_tile.save(os.path.join(icons_dir, "icon.ico"), format="ICO", sizes=ico_sizes)
    logo_512.save(os.path.join(icons_dir, "icon.png"), "PNG")
    logo_128.save(os.path.join(icons_dir, "128x128.png"), "PNG")
    logo_256.save(os.path.join(icons_dir, "128x128@2x.png"), "PNG")
    logo_32.save(os.path.join(icons_dir, "32x32.png"), "PNG")

    # 2. assets dirs
    for sub in ["src/assets", "public/assets", "assets"]:
        p = os.path.join(base, sub)
        os.makedirs(p, exist_ok=True)
        logo_512.save(os.path.join(p, "logo.png"), "PNG")
        ico_tile.save(os.path.join(p, "icon.ico"), format="ICO", sizes=ico_sizes)

    # 3. dist/assets if dist exists
    dist_assets = os.path.join(base, "dist", "assets")
    if os.path.exists(dist_assets):
        logo_512.save(os.path.join(dist_assets, "logo.png"), "PNG")
        ico_tile.save(os.path.join(dist_assets, "icon.ico"), format="ICO", sizes=ico_sizes)

print("Updated all Windows desktop assets & Tauri icons!")

# Update Android resources as well
android_res = r"C:\Users\Atul\Desktop\Aerosync\platform\android\app\src\main\res"
drawable_dir = os.path.join(android_res, "drawable")
os.makedirs(drawable_dir, exist_ok=True)
logo_512.save(os.path.join(drawable_dir, "aerosync_logo.png"), "PNG")

fg_adaptive = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
fg_size = int(512 * 0.68)
fg_inner = master_1024.resize((fg_size, fg_size), Image.Resampling.LANCZOS)
fg_offset = ((512 - fg_size) // 2, (512 - fg_size) // 2)
fg_adaptive.paste(fg_inner, fg_offset, fg_inner)
fg_adaptive.save(os.path.join(drawable_dir, "ic_launcher_foreground.png"), "PNG")

densities = {
    "mipmap-mdpi": 48,
    "mipmap-hdpi": 72,
    "mipmap-xhdpi": 96,
    "mipmap-xxhdpi": 144,
    "mipmap-xxxhdpi": 192
}

for folder, size in densities.items():
    folder_path = os.path.join(android_res, folder)
    os.makedirs(folder_path, exist_ok=True)

    launcher_tile = Image.new("RGBA", (size, size), (10, 14, 26, 255))
    icon_inner_size = int(size * 0.72)
    icon_inner = master_1024.resize((icon_inner_size, icon_inner_size), Image.Resampling.LANCZOS)
    offset = ((size - icon_inner_size) // 2, (size - icon_inner_size) // 2)
    launcher_tile.paste(icon_inner, offset, icon_inner)

    # Square icon
    launcher_tile.save(os.path.join(folder_path, "ic_launcher.png"), "PNG")

    # Round icon
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse((0, 0, size, size), fill=255)
    round_tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    round_tile.paste(launcher_tile, (0, 0), mask=mask)
    round_tile.save(os.path.join(folder_path, "ic_launcher_round.png"), "PNG")

print("Updated all Android icons & drawables!")
print("ALL NEW CLOUD ICONS SUCCESSFULLY GENERATED AND PLACED!")
