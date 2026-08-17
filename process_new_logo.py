import os
import sys
import numpy as np
from PIL import Image, ImageOps, ImageDraw, ImageFilter

src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\fd780606-5c8c-405c-9ff3-3fd836c7c786\.user_uploaded\media_1786866471866.png"
android_res = r"C:\Users\Atul\Desktop\Aerosync\platform\android\app\src\main\res"
windows_assets = r"C:\Users\Atul\Desktop\Aerosync\platform\windows\desktop_app\assets"

print("Loading uploaded logo from:", src_path)
img = Image.open(src_path).convert("RGBA")
width, height = img.size

# Convert to grayscale to extract clean alpha mask
gray = ImageOps.grayscale(img)
arr_gray = np.array(gray, dtype=np.float32)

# Estimate background luminance from the 4 corners
corners = [arr_gray[0:10, 0:10], arr_gray[0:10, -10:], arr_gray[-10:, 0:10], arr_gray[-10:, -10:]]
bg_val = np.mean([np.mean(c) for c in corners])
print("Estimated BG luminance:", bg_val)

# Normalize luminance (background -> 0.0, dark emblem -> 1.0)
norm = (bg_val - arr_gray) / (bg_val - 25.0)
norm = np.clip(norm, 0.0, 1.0)

# Soft thresholding for smooth anti-aliased edges
norm = np.where(norm > 0.12, (norm - 0.12) / 0.88, 0.0)
norm = np.power(norm, 1.05)
norm = np.clip(norm, 0.0, 1.0)

# Create 1024x1024 high-res mask
mask_1024 = Image.fromarray((norm * 255).astype(np.uint8), mode='L').resize((1024, 1024), Image.Resampling.LANCZOS)

# Create vibrant AeroSync Cyan-to-Sky-Blue gradient for the emblem
grad_arr = np.zeros((1024, 1024, 4), dtype=np.uint8)
for y in range(1024):
    for x in range(1024):
        t = (x + y) / 2048.0
        # Vibrant Cyan (#38BDF8) to Electric Blue (#0284C7)
        r = int(56 * (1 - t) + 2 * t)
        g = int(189 * (1 - t) + 132 * t)
        b = int(248 * (1 - t) + 199 * t)
        grad_arr[y, x] = [r, g, b, 255]

grad_img = Image.fromarray(grad_arr, mode='RGBA')
emblem_1024 = Image.new('RGBA', (1024, 1024), (0, 0, 0, 0))
emblem_1024.paste(grad_img, (0, 0), mask=mask_1024)

# Create High-Res 512x512 Master Logo (Transparent background with Cyan/Blue gradient)
logo_512 = emblem_1024.resize((512, 512), Image.Resampling.LANCZOS)

# 1. Save Android in-app logo: drawable/aerosync_logo.png
drawable_dir = os.path.join(android_res, "drawable")
os.makedirs(drawable_dir, exist_ok=True)
logo_path = os.path.join(drawable_dir, "aerosync_logo.png")
logo_512.save(logo_path, "PNG")
print("Saved Android in-app logo to:", logo_path)

# 2. Save Android adaptive foreground: drawable/ic_launcher_foreground.png
# Android Adaptive icon specs: 108dp canvas with 72dp safe zone (66% size centered)
fg_adaptive = Image.new("RGBA", (512, 512), (0, 0, 0, 0))
fg_size = int(512 * 0.68)
fg_inner = logo_512.resize((fg_size, fg_size), Image.Resampling.LANCZOS)
fg_offset = ((512 - fg_size) // 2, (512 - fg_size) // 2)
fg_adaptive.paste(fg_inner, fg_offset, fg_inner)
fg_path = os.path.join(drawable_dir, "ic_launcher_foreground.png")
fg_adaptive.save(fg_path, "PNG")
print("Saved Android adaptive foreground to:", fg_path)

# 3. Generate Android Mipmap Densities for standard launcher icons
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

    # Launcher Icon with Sleek Dark Navy Background (#0F172A)
    launcher_tile = Image.new("RGBA", (size, size), (15, 23, 42, 255))
    icon_inner_size = int(size * 0.72)
    icon_inner = logo_512.resize((icon_inner_size, icon_inner_size), Image.Resampling.LANCZOS)
    offset = ((size - icon_inner_size) // 2, (size - icon_inner_size) // 2)
    launcher_tile.paste(icon_inner, offset, icon_inner)

    # 3a. Square / Squircle Icon
    sq_path = os.path.join(folder_path, "ic_launcher.png")
    launcher_tile.save(sq_path, "PNG")

    # 3b. Round Launcher Icon (with circle mask)
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse((0, 0, size, size), fill=255)
    round_tile = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    round_tile.paste(launcher_tile, (0, 0), mask=mask)
    round_path = os.path.join(folder_path, "ic_launcher_round.png")
    round_tile.save(round_path, "PNG")
    print(f"Saved {folder} icons ({size}x{size})")

# 4. Save Windows Desktop App assets
os.makedirs(windows_assets, exist_ok=True)
win_logo_path = os.path.join(windows_assets, "logo.png")
logo_512.save(win_logo_path, "PNG")
print("Saved Windows Desktop logo to:", win_logo_path)

# Generate Windows .ico with multi-resolution layers
ico_path = os.path.join(windows_assets, "icon.ico")
ico_tile = Image.new("RGBA", (256, 256), (15, 23, 42, 255))
ico_inner = logo_512.resize((190, 190), Image.Resampling.LANCZOS)
ico_tile.paste(ico_inner, (33, 33), ico_inner)

ico_sizes = [(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)]
ico_tile.save(ico_path, format="ICO", sizes=ico_sizes)
print("Saved Windows multi-resolution icon.ico to:", ico_path)

print("\nSUCCESS: All AeroSync app icon assets updated successfully from the new logo!")
