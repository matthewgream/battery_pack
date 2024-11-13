Import("env")
import os

platform = "pio" + env.get('PIOFRAMEWORK', ['unknown'])[0].lower()
hardware = env.get('BOARD', 'unknown').replace('-', '').lower()
os.environ['PIOBUILD_PLATFORM'] = platform
os.environ['PIOBUILD_HARDWARE'] = hardware
env['PIOBUILD_PLATFORM'] = platform
env['PIOBUILD_HARDWARE'] = hardware
print(f"OTA_IMAGE_UPLOAD: prepare platform={platform}, hardware={hardware}")
env.Append(CXXFLAGS=[f"-DBUILD_PLATFORM=\\\"{platform}\\\"", f"-DBUILD_HARDWARE=\\\"{hardware}\\\""])

env.Append(CXXFLAGS=["-std=gnu++2a", "-fconcepts"])