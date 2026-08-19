import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageOps, ImageFilter

root_dir = r"C:\Users\Atul\Desktop\Aerosync"

# Source transparent emblem
src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\a34e0db2-8caa-4723-8cc5-e5a5d60048b2\.user_uploaded\media_1787111215101.png"
if not os.path.exists(src_path):
    # Fallback to local scratch
    src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\69ddd08d-af07-4300-a651-ab07e4cf2dc0\scratch\img_3.png"

print(f"Loading official transparent AeroSync emblem from: {src_path}")
raw_emblem = Image.open(src_path).convert("RGBA")
bbox = raw_emblem.getbbox()
if bbox:
    cropped_emblem = raw_emblem.crop(bbox)
else:
    cropped_emblem = raw_emblem

ew, eh = cropped_emblem.size
print(f"Cropped emblem dimensions: {ew}x{eh}")

# -------------------------------------------------------------
# 1. Master 1024x1024 Transparent Canvas
# -------------------------------------------------------------
master_transparent_1024 = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
target_dim = 920
scale = min(target_dim / ew, target_dim / eh)
new_w, new_h = int(round(ew * scale)), int(round(eh * scale))
scaled_emblem = cropped_emblem.resize((new_w, new_h), Image.Resampling.LANCZOS)
paste_pos = ((1024 - new_w) // 2, (1024 - new_h) // 2)
master_transparent_1024.paste(scaled_emblem, paste_pos, scaled_emblem)

# Standard sizes (all transparent background)
logo_512 = master_transparent_1024.resize((512, 512), Image.Resampling.LANCZOS)
logo_256 = master_transparent_1024.resize((256, 256), Image.Resampling.LANCZOS)
logo_128 = master_transparent_1024.resize((128, 128), Image.Resampling.LANCZOS)
logo_64 = master_transparent_1024.resize((64, 64), Image.Resampling.LANCZOS)
logo_48 = master_transparent_1024.resize((48, 48), Image.Resampling.LANCZOS)
logo_32 = master_transparent_1024.resize((32, 32), Image.Resampling.LANCZOS)
logo_24 = master_transparent_1024.resize((24, 24), Image.Resampling.LANCZOS)
logo_16 = master_transparent_1024.resize((16, 16), Image.Resampling.LANCZOS)

ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]

# Helper to save multi-resolution ICO with full 32-bit RGBA transparency
def save_multi_res_ico(dest_path):
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
    master_transparent_1024.save(dest_path, format="ICO", sizes=ico_sizes)
    print(f"Saved transparent ICO: {dest_path}")

# Helper for Android badge launcher icons (with dark navy badge and transparent corner mask)
def make_android_tile(size, round_shape=False):
    tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(tile)
    navy_bg = (15, 23, 42, 255) # Deep sleek navy #0F172A

    if round_shape:
        draw.ellipse((0, 0, size, size), fill=navy_bg)
    else:
        radius = int(size * 0.22)
        draw.rounded_rectangle((0, 0, size, size), radius=radius, fill=navy_bg)

    inner_sz = int(size * 0.72)
    inner_logo = master_transparent_1024.resize((inner_sz, inner_sz), Image.Resampling.LANCZOS)
    pos = ((size - inner_sz) // 2, (size - inner_sz) // 2)
    tile.paste(inner_logo, pos, inner_logo)
    return tile

# -------------------------------------------------------------
# 2. Update Windows Native Assets
# -------------------------------------------------------------
win_assets_dir = os.path.join(root_dir, "platform", "windows", "assets")
os.makedirs(win_assets_dir, exist_ok=True)
save_multi_res_ico(os.path.join(win_assets_dir, "icon.ico"))
logo_512.save(os.path.join(win_assets_dir, "logo.png"), "PNG")
logo_512.save(os.path.join(win_assets_dir, "icon.png"), "PNG")
logo_256.save(os.path.join(win_assets_dir, "256x256.png"), "PNG")
logo_128.save(os.path.join(win_assets_dir, "128x128.png"), "PNG")
logo_128.save(os.path.join(win_assets_dir, "128x128@2x.png"), "PNG")
logo_64.save(os.path.join(win_assets_dir, "64x64.png"), "PNG")
logo_48.save(os.path.join(win_assets_dir, "48x48.png"), "PNG")
logo_32.save(os.path.join(win_assets_dir, "32x32.png"), "PNG")
logo_24.save(os.path.join(win_assets_dir, "24x24.png"), "PNG")
logo_16.save(os.path.join(win_assets_dir, "16x16.png"), "PNG")

win_src_assets = os.path.join(root_dir, "platform", "windows", "src", "assets")
if os.path.exists(win_src_assets):
    save_multi_res_ico(os.path.join(win_src_assets, "icon.ico"))

print("Updated Windows native assets with transparent AeroSync branding!")

# -------------------------------------------------------------
# 3. Update Tauri & Web App Icon Assets
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
    icons_dir = os.path.join(base, "src-tauri", "icons")
    os.makedirs(icons_dir, exist_ok=True)
    save_multi_res_ico(os.path.join(icons_dir, "icon.ico"))
    logo_512.save(os.path.join(icons_dir, "icon.png"), "PNG")
    logo_512.save(os.path.join(icons_dir, "logo.png"), "PNG")
    logo_256.save(os.path.join(icons_dir, "256x256.png"), "PNG")
    logo_128.save(os.path.join(icons_dir, "128x128.png"), "PNG")
    logo_128.save(os.path.join(icons_dir, "128x128@2x.png"), "PNG")
    logo_64.save(os.path.join(icons_dir, "64x64.png"), "PNG")
    logo_48.save(os.path.join(icons_dir, "48x48.png"), "PNG")
    logo_32.save(os.path.join(icons_dir, "32x32.png"), "PNG")
    logo_24.save(os.path.join(icons_dir, "24x24.png"), "PNG")
    logo_16.save(os.path.join(icons_dir, "16x16.png"), "PNG")

    for sname, sz in tauri_sizes_map.items():
        master_transparent_1024.resize((sz, sz), Image.Resampling.LANCZOS).save(os.path.join(icons_dir, sname), "PNG")

    for sub in ["src/assets", "public/assets", "dist/assets"]:
        p = os.path.join(base, sub)
        if os.path.exists(p) or sub != "dist/assets":
            os.makedirs(p, exist_ok=True)
            logo_512.save(os.path.join(p, "logo.png"), "PNG")
            save_multi_res_ico(os.path.join(p, "icon.ico"))
            logo_512.save(os.path.join(p, "icon.png"), "PNG")
            logo_256.save(os.path.join(p, "256x256.png"), "PNG")
            logo_128.save(os.path.join(p, "128x128.png"), "PNG")
            logo_128.save(os.path.join(p, "128x128@2x.png"), "PNG")
            logo_64.save(os.path.join(p, "64x64.png"), "PNG")
            logo_48.save(os.path.join(p, "48x48.png"), "PNG")
            logo_32.save(os.path.join(p, "32x32.png"), "PNG")
            logo_24.save(os.path.join(p, "24x24.png"), "PNG")
            logo_16.save(os.path.join(p, "16x16.png"), "PNG")

print("Updated all Tauri & Web asset icon files with transparent AeroSync branding!")

# -------------------------------------------------------------
# 4. Update Android Mobile Application Assets
# -------------------------------------------------------------
android_res = os.path.join(root_dir, "platform", "android", "app", "src", "main", "res")
drawable_dir = os.path.join(android_res, "drawable")
os.makedirs(drawable_dir, exist_ok=True)

# In-app transparent logo
logo_512.save(os.path.join(drawable_dir, "aerosync_logo.png"), "PNG")

# Adaptive foreground (108dp canvas with safe area centered emblem)
fg_adaptive = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
fg_sz = int(512 * 0.68)
fg_img = master_transparent_1024.resize((fg_sz, fg_sz), Image.Resampling.LANCZOS)
fg_pos = ((512 - fg_sz) // 2, (512 - fg_sz) // 2)
fg_adaptive.paste(fg_img, fg_pos, fg_img)
fg_adaptive.save(os.path.join(drawable_dir, "ic_launcher_foreground.png"), "PNG")

# Android mipmap launcher icons
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

    # Square / Squircle with transparent corners
    sq_tile = make_android_tile(size, round_shape=False)
    sq_tile.save(os.path.join(folder_path, "ic_launcher.png"), "PNG")

    # Round tile with transparent corners
    round_tile = make_android_tile(size, round_shape=True)
    round_tile.save(os.path.join(folder_path, "ic_launcher_round.png"), "PNG")

print("Updated Android launcher mipmaps & drawables!")
print("ALL ICONS SUCCESSFULLY REGENERATED WITH CLEAN TRANSPARENCY!")
