import os
import sys
import numpy as np
from PIL import Image, ImageDraw, ImageOps

src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\a34e0db2-8caa-4723-8cc5-e5a5d60048b2\.user_uploaded\media_1787111253738.png"
root_dir = r"C:\Users\Atul\Desktop\Aerosync"

print("Loading 5th picture from:", src_path)
pic5 = Image.open(src_path).convert("RGBA")
pw, ph = pic5.size
print(f"5th picture original dimensions: {pw}x{ph}")

# Estimate dark navy background color from corners
arr = np.array(pic5, dtype=np.float32)
corners = [arr[0:5, 0:5, :3], arr[0:5, -5:, :3], arr[-5:, 0:5, :3], arr[-5:, -5:, :3]]
bg_color = np.mean(corners, axis=(0, 1, 2))
bg_tuple = (int(round(bg_color[0])), int(round(bg_color[1])), int(round(bg_color[2])), 255)
print(f"Detected 5th picture BG RGB: {bg_tuple[:3]}")

# Crop tight bounding box around logo if any extra borders exist
bbox = pic5.getbbox()
if bbox:
    cropped_pic5 = pic5.crop(bbox)
else:
    cropped_pic5 = pic5

cw, ch = cropped_pic5.size

# Master 1024x1024 Square Tile containing exact 5th picture
master_tile_1024 = Image.new("RGBA", (1024, 1024), bg_tuple)
# Scale 5th picture to fit neatly with slight padding
scale = min(960.0 / cw, 960.0 / ch)
new_w, new_h = int(cw * scale), int(ch * scale)
scaled_pic5 = cropped_pic5.resize((new_w, new_h), Image.Resampling.LANCZOS)
paste_pos = ((1024 - new_w) // 2, (1024 - new_h) // 2)
master_tile_1024.paste(scaled_pic5, paste_pos, scaled_pic5 if scaled_pic5.mode == 'RGBA' else None)

# Also create master transparent emblem for Android adaptive foreground
dist = np.linalg.norm(arr[:, :, :3] - bg_color, axis=2)
alpha = np.clip((dist - 12.0) / 130.0, 0.0, 1.0)
alpha = np.power(alpha, 0.85)

rgb_clean = np.zeros_like(arr[:, :, :3])
for c in range(3):
    rgb_clean[:, :, c] = np.clip(
        (arr[:, :, c] - bg_color[c] * (1.0 - alpha)) / np.maximum(alpha, 0.001),
        0.0, 255.0
    )

emblem_rgba = np.dstack([rgb_clean, alpha * 255.0]).astype(np.uint8)
extracted_emblem = Image.fromarray(emblem_rgba, "RGBA")
ebbox = extracted_emblem.getbbox()
cropped_emblem = extracted_emblem.crop(ebbox) if ebbox else extracted_emblem

master_emblem_1024 = Image.new("RGBA", (1024, 1024), (0, 0, 0, 0))
ew, eh = cropped_emblem.size
escale = min(840.0 / ew, 840.0 / eh)
enew_w, enew_h = int(ew * escale), int(eh * escale)
scaled_emblem = cropped_emblem.resize((enew_w, enew_h), Image.Resampling.LANCZOS)
master_emblem_1024.paste(scaled_emblem, ((1024 - enew_w) // 2, (1024 - enew_h) // 2), scaled_emblem)

# Helper function to generate resized square tiles
def make_tile(size, round_shape=False):
    tile = Image.new("RGBA", (size, size), bg_tuple if not round_shape else (0,0,0,0))
    if round_shape:
        # Create round mask
        mask = Image.new("L", (size, size), 0)
        draw = ImageDraw.Draw(mask)
        draw.ellipse((0, 0, size, size), fill=255)
        
        sq = master_tile_1024.resize((size, size), Image.Resampling.LANCZOS)
        tile.paste(sq, (0, 0), mask=mask)
    else:
        tile = master_tile_1024.resize((size, size), Image.Resampling.LANCZOS)
    return tile

# Resized images
logo_512 = master_tile_1024.resize((512, 512), Image.Resampling.LANCZOS)
logo_256 = master_tile_1024.resize((256, 256), Image.Resampling.LANCZOS)
logo_128 = master_tile_1024.resize((128, 128), Image.Resampling.LANCZOS)
logo_64 = master_tile_1024.resize((64, 64), Image.Resampling.LANCZOS)
logo_48 = master_tile_1024.resize((48, 48), Image.Resampling.LANCZOS)
logo_32 = master_tile_1024.resize((32, 32), Image.Resampling.LANCZOS)
logo_24 = master_tile_1024.resize((24, 24), Image.Resampling.LANCZOS)
logo_16 = master_tile_1024.resize((16, 16), Image.Resampling.LANCZOS)

ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (24, 24), (16, 16)]

# -------------------------------------------------------------
# 1. Windows Native Assets
# -------------------------------------------------------------
win_assets_dir = os.path.join(root_dir, "platform", "windows", "assets")
os.makedirs(win_assets_dir, exist_ok=True)

logo_256.save(os.path.join(win_assets_dir, "icon.ico"), format="ICO", sizes=ico_sizes)
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

print("Updated Windows native assets with exact 5th picture!")

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
    icons_dir = os.path.join(base, "src-tauri", "icons")
    os.makedirs(icons_dir, exist_ok=True)
    logo_256.save(os.path.join(icons_dir, "icon.ico"), format="ICO", sizes=ico_sizes)
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
        make_tile(sz).save(os.path.join(icons_dir, sname), "PNG")

    for sub in ["src/assets", "public/assets"]:
        p = os.path.join(base, sub)
        os.makedirs(p, exist_ok=True)
        logo_512.save(os.path.join(p, "logo.png"), "PNG")
        logo_256.save(os.path.join(p, "icon.ico"), format="ICO", sizes=ico_sizes)
        logo_512.save(os.path.join(p, "icon.png"), "PNG")
        logo_256.save(os.path.join(p, "256x256.png"), "PNG")
        logo_128.save(os.path.join(p, "128x128.png"), "PNG")
        logo_128.save(os.path.join(p, "128x128@2x.png"), "PNG")
        logo_64.save(os.path.join(p, "64x64.png"), "PNG")
        logo_48.save(os.path.join(p, "48x48.png"), "PNG")
        logo_32.save(os.path.join(p, "32x32.png"), "PNG")
        logo_24.save(os.path.join(p, "24x24.png"), "PNG")
        logo_16.save(os.path.join(p, "16x16.png"), "PNG")

print("Updated Tauri & Web asset icon files with exact 5th picture!")

# -------------------------------------------------------------
# 3. Android Mobile Application Assets
# -------------------------------------------------------------
android_res = os.path.join(root_dir, "platform", "android", "app", "src", "main", "res")
drawable_dir = os.path.join(android_res, "drawable")
os.makedirs(drawable_dir, exist_ok=True)

# In-app logo (exact 5th picture)
logo_512.save(os.path.join(drawable_dir, "aerosync_logo.png"), "PNG")

# Adaptive foreground
fg_adaptive = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
fg_sz = int(512 * 0.68)
fg_img = master_emblem_1024.resize((fg_sz, fg_sz), Image.Resampling.LANCZOS)
fg_offset = ((512 - fg_sz) // 2, (512 - fg_sz) // 2)
fg_adaptive.paste(fg_img, fg_offset, fg_img)
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

    sq_tile = make_tile(size, round_shape=False)
    sq_tile.save(os.path.join(folder_path, "ic_launcher.png"), "PNG")

    round_tile = make_tile(size, round_shape=True)
    round_tile.save(os.path.join(folder_path, "ic_launcher_round.png"), "PNG")

print("Updated Android launcher mipmaps & drawables with exact 5th picture!")
print("EXACT 5TH PICTURE APPLIED EVERYWHERE SUCCESSFULLY!")
