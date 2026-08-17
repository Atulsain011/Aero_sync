import sys
from PIL import Image, ImageDraw, ImageOps
import os

src_path = r"C:\Users\Atul\.gemini\antigravity-ide\brain\7249d6e9-0612-4b39-b346-3fd7139389a3\.user_uploaded\media_1786735900644.png"
res_dir = r"C:\Users\Atul\Desktop\Aerosync\platform\android\app\src\main\res"

img = Image.open(src_path).convert("RGBA")
width, height = img.size

# Threshold to make white/near-white background transparent
datas = img.getdata()
new_data = []
for item in datas:
    r, g, b, a = item
    # If the pixel is near white (r, g, b > 235), make it transparent
    if r > 235 and g > 235 and b > 235:
        new_data.append((255, 255, 255, 0))
    else:
        new_data.append(item)

img.putdata(new_data)

# Find bounding box of non-transparent pixels to crop tightly
bbox = img.getbbox()
if bbox:
    cropped = img.crop(bbox)
else:
    cropped = img

# Create square image with padding
max_dim = max(cropped.width, cropped.height)
pad = int(max_dim * 0.08) # 8% padding
sq_size = max_dim + pad * 2
square_img = Image.new("RGBA", (sq_size, sq_size), (0, 0, 0, 0))
offset = ((sq_size - cropped.width) // 2, (sq_size - cropped.height) // 2)
square_img.paste(cropped, offset, cropped)

# Resize to high-res 512x512
logo_512 = square_img.resize((512, 512), Image.Resampling.LANCZOS)
drawable_path = os.path.join(res_dir, "drawable", "aerosync_logo.png")
os.makedirs(os.path.dirname(drawable_path), exist_ok=True)
logo_512.save(drawable_path, "PNG")
print("Saved transparent logo emblem to", drawable_path)

# Generate Android launcher icons
# Sizes for mipmap densities
densities = {
    "mipmap-mdpi": 48,
    "mipmap-hdpi": 72,
    "mipmap-xhdpi": 96,
    "mipmap-xxhdpi": 144,
    "mipmap-xxxhdpi": 192
}

for folder, size in densities.items():
    folder_path = os.path.join(res_dir, folder)
    os.makedirs(folder_path, exist_ok=True)
    
    # 1. Standard square launcher icon with subtle background
    bg_sq = Image.new("RGBA", (size, size), (255, 255, 255, 255))
    icon_resized = square_img.resize((int(size * 0.85), int(size * 0.85)), Image.Resampling.LANCZOS)
    icon_offset = ((size - icon_resized.width) // 2, (size - icon_resized.height) // 2)
    bg_sq.paste(icon_resized, icon_offset, icon_resized)
    bg_sq.save(os.path.join(folder_path, "ic_launcher.png"), "PNG")
    
    # 2. Round launcher icon
    bg_round = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse((0, 0, size, size), fill=255)
    
    white_circle = Image.new("RGBA", (size, size), (255, 255, 255, 255))
    white_circle.paste(icon_resized, icon_offset, icon_resized)
    bg_round.paste(white_circle, (0, 0), mask=mask)
    bg_round.save(os.path.join(folder_path, "ic_launcher_round.png"), "PNG")
    print(f"Saved {folder} launcher icons ({size}x{size})")

print("All logo icons generated successfully!")
