import os
import sys
import numpy as np
from PIL import Image, ImageOps, ImageDraw, ImageFilter

src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\a34e0db2-8caa-4723-8cc5-e5a5d60048b2\.user_uploaded\media_1787109030206.png"
root_dir = r"C:\Users\Atul\Desktop\Aerosync"

print("Loading official AeroSync logo reference from:", src_path)
raw_img = Image.open(src_path).convert("RGBA")
w, h = raw_img.size
print(f"Original image dimensions: {w}x{h}")

arr = np.array(raw_img, dtype=np.float32)

# Estimate dark navy background color from corners
corners = [arr[0:10, 0:10, :3], arr[0:10, -10:, :3], arr[-10:, 0:10, :3], arr[-10:, -10:, :3]]
bg_color = np.mean(corners, axis=(0, 1, 2))
print(f"Estimated Background RGB: ({bg_color[0]:.1f}, {bg_color[1]:.1f}, {bg_color[2]:.1f})")

# Calculate color distance to isolate cloud emblem and arrows
dist = np.linalg.norm(arr[:, :, :3] - bg_color, axis=2)
alpha = np.clip((dist - 15.0) / 140.0, 0.0, 1.0)
alpha = np.power(alpha, 0.85)

# Clean RGB unmult
rgb_clean = np.zeros_like(arr[:, :, :3])
for c in range(3):
    rgb_clean[:, :, c] = np.clip(
        (arr[:, :, c] - bg_color[c] * (1.0 - alpha)) / np.maximum(alpha, 0.001),
        0.0, 255.0
    )

rgba_emblem = np.dstack([rgb_clean, alpha * 255.0]).astype(np.uint8)
extracted_emblem = Image.fromarray(rgba_emblem, "RGBA")

bbox = extracted_emblem.getbbox()
if bbox:
    cropped_emblem = extracted_emblem.crop(bbox)
else:
    cropped_emblem = extracted_emblem

# Master 1024x1024 transparent canvas
master_1024 = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
ew, eh = cropped_emblem.size
scale = min(840.0 / ew, 840.0 / eh)
new_w, new_h = int(ew * scale), int(eh * scale)
scaled_emblem = cropped_emblem.resize((new_w, new_h), Image.Resampling.LANCZOS)
paste_pos = ((1024 - new_w) // 2, (1024 - new_h) // 2)
master_1024.paste(scaled_emblem, paste_pos, scaled_emblem)

# Master 512x512 transparent logo
logo_512 = master_1024.resize((512, 512), Image.Resampling.LANCZOS)
logo_256 = master_1024.resize((256, 256), Image.Resampling.LANCZOS)
logo_128 = master_1024.resize((128, 128), Image.Resampling.LANCZOS)
logo_64 = master_1024.resize((64, 64), Image.Resampling.LANCZOS)
logo_48 = master_1024.resize((48, 48), Image.Resampling.LANCZOS)
logo_32 = master_1024.resize((32, 32), Image.Resampling.LANCZOS)
logo_24 = master_1024.resize((24, 24), Image.Resampling.LANCZOS)
logo_16 = master_1024.resize((16, 16), Image.Resampling.LANCZOS)

# Create Dark Navy Application Tile (matching reference background #090D16)
def create_app_tile(size, round_shape=False):
    tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tile)
    navy_bg = (9, 13, 22, 255)

    if round_shape:
        draw.ellipse((0, 0, size, size), fill=navy_bg)
    else:
        radius = int(size * 0.22)
        draw.rounded_rectangle((0, 0, size, size), radius=radius, fill=navy_bg)

    inner_sz = int(size * 0.74)
    inner_img = master_1024.resize((inner_sz, inner_sz), Image.Resampling.LANCZOS)
    offset = ((size - inner_sz) // 2, (size - inner_sz) // 2)
    tile.paste(inner_img, offset, inner_img)
    return tile

# -------------------------------------------------------------
# 1. Windows Native Assets & ICO Files
# -------------------------------------------------------------
win_assets_dir = os.path.join(root_dir, "platform", "windows", "assets")
os.makedirs(win_assets_dir, exist_ok=True)

ico_master_256 = create_app_tile(256, round_shape=False)
ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]

# Save Windows multi-res ICO and PNGs
ico_master_256.save(os.path.join(win_assets_dir, "icon.ico"), format="ICO", sizes=ico_sizes)
logo_512.save(os.path.join(win_assets_dir, "logo.png"), "PNG")
ico_master_256.save(os.path.join(win_assets_dir, "icon.png"), "PNG")
logo_256.save(os.path.join(win_assets_dir, "256x256.png"), "PNG")
logo_128.save(os.path.join(win_assets_dir, "128x128.png"), "PNG")
logo_128.save(os.path.join(win_assets_dir, "128x128@2x.png"), "PNG")
logo_64.save(os.path.join(win_assets_dir, "64x64.png"), "PNG")
logo_48.save(os.path.join(win_assets_dir, "48x48.png"), "PNG")
logo_32.save(os.path.join(win_assets_dir, "32x32.png"), "PNG")
logo_24.save(os.path.join(win_assets_dir, "24x24.png"), "PNG")
logo_16.save(os.path.join(win_assets_dir, "16x16.png"), "PNG")

print("Generated Windows native assets & multi-resolution icon.ico in:", win_assets_dir)

# -------------------------------------------------------------
# 2. Tauri & Web App Icon Assets
# -------------------------------------------------------------
tauri_dirs = [
    os.path.join(root_dir, "platform", "windows", "desktop_tauri"),
    os.path.join(root_dir, "platform", "windows", "desktop_app")
]

tauri_sizes_map = {
    "Square30x30Logo.png": 30,
    "Square44x44Logo.png": 44,
    "Square71x71Logo.png": 71,
    "Square89x89Logo.png": 89,
    "Square107x107Logo.png": 107,
    "Square142x142Logo.png": 142,
    "Square150x150Logo.png": 150,
    "Square284x284Logo.png": 284,
    "Square310x310Logo.png": 310,
    "StoreLogo.png": 50
}

for base in tauri_dirs:
    # icons dir
    icons_dir = os.path.join(base, "src-tauri", "icons")
    os.makedirs(icons_dir, exist_ok=True)
    ico_master_256.save(os.path.join(icons_dir, "icon.ico"), format="ICO", sizes=ico_sizes)
    create_app_tile(512).save(os.path.join(icons_dir, "icon.png"), "PNG")
    logo_512.save(os.path.join(icons_dir, "logo.png"), "PNG")
    create_app_tile(256).save(os.path.join(icons_dir, "256x256.png"), "PNG")
    create_app_tile(128).save(os.path.join(icons_dir, "128x128.png"), "PNG")
    create_app_tile(256).save(os.path.join(icons_dir, "128x128@2x.png"), "PNG")
    create_app_tile(64).save(os.path.join(icons_dir, "64x64.png"), "PNG")
    create_app_tile(48).save(os.path.join(icons_dir, "48x48.png"), "PNG")
    create_app_tile(32).save(os.path.join(icons_dir, "32x32.png"), "PNG")
    create_app_tile(24).save(os.path.join(icons_dir, "24x24.png"), "PNG")
    create_app_tile(16).save(os.path.join(icons_dir, "16x16.png"), "PNG")

    for sname, sz in tauri_sizes_map.items():
        create_app_tile(sz).save(os.path.join(icons_dir, sname), "PNG")

    # src/assets & public/assets
    for sub in ["src/assets", "public/assets"]:
        p = os.path.join(base, sub)
        os.makedirs(p, exist_ok=True)
        logo_512.save(os.path.join(p, "logo.png"), "PNG")
        ico_master_256.save(os.path.join(p, "icon.ico"), format="ICO", sizes=ico_sizes)
        logo_512.save(os.path.join(p, "icon.png"), "PNG")
        logo_256.save(os.path.join(p, "256x256.png"), "PNG")
        logo_128.save(os.path.join(p, "128x128.png"), "PNG")
        logo_128.save(os.path.join(p, "128x128@2x.png"), "PNG")
        logo_64.save(os.path.join(p, "64x64.png"), "PNG")
        logo_48.save(os.path.join(p, "48x48.png"), "PNG")
        logo_32.save(os.path.join(p, "32x32.png"), "PNG")
        logo_24.save(os.path.join(p, "24x24.png"), "PNG")
        logo_16.save(os.path.join(p, "16x16.png"), "PNG")

print("Generated all Tauri & Web asset icon files!")

# -------------------------------------------------------------
# 3. Android Mobile Application Assets
# -------------------------------------------------------------
android_res = os.path.join(root_dir, "platform", "android", "app", "src", "main", "res")
drawable_dir = os.path.join(android_res, "drawable")
os.makedirs(drawable_dir, exist_ok=True)

# In-app logo
logo_512.save(os.path.join(drawable_dir, "aerosync_logo.png"), "PNG")

# Adaptive foreground (108dp canvas, safe area 66%)
fg_adaptive = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
fg_sz = int(512 * 0.66)
fg_img = master_1024.resize((fg_sz, fg_sz), Image.Resampling.LANCZOS)
fg_offset = ((512 - fg_sz) // 2, (512 - fg_sz) // 2)
fg_adaptive.paste(fg_img, fg_offset, fg_img)
fg_adaptive.save(os.path.join(drawable_dir, "ic_launcher_foreground.png"), "PNG")

# Android mipmap launcher icons across all densities
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

    # Square tile
    sq_tile = create_app_tile(size, round_shape=False)
    sq_tile.save(os.path.join(folder_path, "ic_launcher.png"), "PNG")

    # Round tile
    round_tile = create_app_tile(size, round_shape=True)
    round_tile.save(os.path.join(folder_path, "ic_launcher_round.png"), "PNG")

print("Generated all Android launcher mipmap assets & adaptive icon drawables!")
print("AEROSYNC OFFICIAL BRANDING GENERATION SUCCESSFUL!")
