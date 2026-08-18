import os
import io
import struct
import numpy as np
from PIL import Image, ImageDraw

src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\1972648b-0a85-418d-9ea7-e295e9dd6fb3\.user_uploaded\media_1787029635197.png"

print("Loading user uploaded cloud logo from:", src_path)
img = Image.open(src_path).convert("RGBA")
width, height = img.size
arr = np.array(img, dtype=np.float32)

# Estimate background color from corners
bg_r = np.mean([arr[0:5, 0:5, 0], arr[0:5, -5:, 0], arr[-5:, 0:5, 0], arr[-5:, -5:, 0]])
bg_g = np.mean([arr[0:5, 0:5, 1], arr[0:5, -5:, 1], arr[-5:, 0:5, 1], arr[-5:, -5:, 1]])
bg_b = np.mean([arr[0:5, 0:5, 2], arr[0:5, -5:, 2], arr[-5:, 0:5, 2], arr[-5:, -5:, 2]])
print(f"Estimated BG RGB: ({bg_r:.1f}, {bg_g:.1f}, {bg_b:.1f})")

# Cyan channel extraction
cyan_intensity = (arr[:, :, 1] - bg_g) * 0.4 + (arr[:, :, 2] - bg_b) * 0.6
cyan_intensity = np.maximum(cyan_intensity, 0.0)

max_cyan = np.max(cyan_intensity)
alpha = cyan_intensity / (max_cyan * 0.75)
alpha = np.clip(alpha, 0.0, 1.0)

# Soft curve for anti-aliasing
alpha = np.where(alpha > 0.1, (alpha - 0.1) / 0.9, 0.0)
alpha = np.power(alpha, 0.9)
alpha = np.clip(alpha * 255.0, 0, 255).astype(np.uint8)

emblem_arr = np.zeros((height, width, 4), dtype=np.uint8)
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

# 1024x1024 master transparent logo
master_1024 = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
crop_w, crop_h = cropped.size
scale = min(820 / crop_w, 820 / crop_h)
new_w = int(crop_w * scale)
new_h = int(crop_h * scale)
resized_crop = cropped.resize((new_w, new_h), Image.Resampling.LANCZOS)
paste_pos = ((1024 - new_w) // 2, (1024 - new_h) // 2)
master_1024.paste(resized_crop, paste_pos, resized_crop)

# 1024x1024 dark navy tile for app icon
tile_1024 = Image.new("RGBA", (1024, 1024), (10, 14, 26, 255))
inner_scale = 0.76
tile_inner_w = int(1024 * inner_scale)
tile_inner_h = int(1024 * inner_scale)
tile_inner = master_1024.resize((tile_inner_w, tile_inner_h), Image.Resampling.LANCZOS)
tile_1024.paste(tile_inner, ((1024 - tile_inner_w) // 2, (1024 - tile_inner_h) // 2), tile_inner)

def create_true_multires_ico(source_tile, out_path, sizes=[256, 128, 64, 48, 32, 24, 16]):
    png_images = []
    for s in sizes:
        resized = source_tile.resize((s, s), Image.Resampling.LANCZOS)
        buf = io.BytesIO()
        resized.save(buf, format='PNG')
        data = buf.getvalue()
        png_images.append((s, s, data))
        
    count = len(png_images)
    header = struct.pack('<HHH', 0, 1, count)
    offset = 6 + count * 16
    dir_entries = []
    data_blobs = []
    
    for w, h, data in png_images:
        w_byte = 0 if w >= 256 else w
        h_byte = 0 if h >= 256 else h
        size = len(data)
        entry = struct.pack('<BBBBHHII', w_byte, h_byte, 0, 0, 1, 32, size, offset)
        dir_entries.append(entry)
        data_blobs.append(data)
        offset += size
        
    with open(out_path, 'wb') as f:
        f.write(header)
        for entry in dir_entries:
            f.write(entry)
        for blob in data_blobs:
            f.write(blob)
    print(f"Generated multi-res ICO ({len(sizes)} layers) -> {out_path}")

target_dirs = [
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_tauri\src-tauri\icons",
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_app\src-tauri\icons"
]

all_png_sizes = [
    ("16x16.png", 16),
    ("24x24.png", 24),
    ("32x32.png", 32),
    ("48x48.png", 48),
    ("64x64.png", 64),
    ("128x128.png", 128),
    ("128x128@2x.png", 256),
    ("256x256.png", 256),
    ("icon.png", 512),
    ("Square30x30Logo.png", 30),
    ("Square44x44Logo.png", 44),
    ("Square71x71Logo.png", 71),
    ("Square89x89Logo.png", 89),
    ("Square107x107Logo.png", 107),
    ("Square142x142Logo.png", 142),
    ("Square150x150Logo.png", 150),
    ("Square284x284Logo.png", 284),
    ("Square310x310Logo.png", 310),
    ("StoreLogo.png", 50)
]

for t_dir in target_dirs:
    os.makedirs(t_dir, exist_ok=True)
    # 1. Multi-resolution icon.ico
    create_true_multires_ico(tile_1024, os.path.join(t_dir, "icon.ico"))
    
    # 2. All required individual PNG sizes
    for fname, size in all_png_sizes:
        res = tile_1024.resize((size, size), Image.Resampling.LANCZOS)
        res.save(os.path.join(t_dir, fname), "PNG")
        
print("Updated all src-tauri/icons!")

# Also update web/desktop asset directories
asset_dirs = [
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_tauri\src\assets",
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_tauri\public\assets",
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_tauri\assets",
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_app\src\assets",
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_app\public\assets",
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_app\assets",
    r"C:\Users\Atul\Desktop\Aerosync\platform\windows\assets"
]

for ad in asset_dirs:
    os.makedirs(ad, exist_ok=True)
    master_1024.resize((512, 512), Image.Resampling.LANCZOS).save(os.path.join(ad, "logo.png"), "PNG")
    create_true_multires_ico(tile_1024, os.path.join(ad, "icon.ico"))

print("All icon files and sizes successfully generated!")
