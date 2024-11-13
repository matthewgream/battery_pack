#!/usr/bin/python

import os
import re
import requests
from pathlib import Path

Import("env")

def extract_info(file_path, patterns):
    print(f"OTA_IMAGE_UPLOAD: extract version from {file_path}")
    if not file_path.exists():
        return None
    with file_path.open('r') as f:
        content = f.read()
    result = {}
    for key, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            result[key] = match.group(1)
        else:
            result[key] = None
    return result

def upload_image(bin_path, url, new_filename, timeout=30):
    print(f"OTA_IMAGE_UPLOAD: upload image {bin_path} as {new_filename} to {url}")
    try:
        with bin_path.open('rb') as f:
            files = {'image': (new_filename, f, 'application/octet-stream')}
            response = requests.put(url, files=files, timeout=timeout)
            if response.status_code == 409 or "File with this name already exists" in response.text:
                print(f"OTA_IMAGE_UPLOAD: upload warning -- file already exists on server: {new_filename}")
                return False
            response.raise_for_status()
            print(f"OTA_IMAGE_UPLOAD: upload success -- {response.text}")
            return True
    except Exception as e:
        print(f"OTA_IMAGE_UPLOAD: upload failure -- {str(e)}")
        return False

def on_built(source, target, env):
    try:
        platform = env.get('PIOBUILD_PLATFORM', 'unknown')
        hardware = env.get('PIOBUILD_HARDWARE', 'unknown')
        info_file = env.GetProjectOption("custom_ota_upload_info_file")
        server = env.GetProjectOption("custom_ota_upload_server").rstrip('/')
        
        if not all([info_file, platform, hardware, server]):
            print("OTA_IMAGE_UPLOAD: error -- missing required custom_ota_upload items in platformio.ini")
            print(f"[custom_ota_upload_ ... info_file={info_file}, platform={platform}, hardware={hardware}, server={server}]")
            return
        
        # Extract version info
        patterns = {
            'name': r'#define\s+DEFAULT_NAME\s+"([^"]+)"',
            'vers': r'#define\s+DEFAULT_VERS\s+"(\d+\.\d+\.\d+)"'
        }
        
        matches = extract_info(Path (info_file), patterns)
        if not matches or None in matches.values():
            print(f"OTA_IMAGE_UPLOAD: error -- couldn't extract name/version from {info_file}")
            return
        
        name = matches['name'].lower()
        vers = matches['vers'].lower()
        filename = f"{name}-{platform}-{hardware}_v{vers}.bin".lower()
        
        cached_path = Path (env.subst("$BUILD_DIR")) / filename
        firmware_path = Path (str(target[0]))
        
        print(f"OTA_IMAGE_UPLOAD: *** image name: {name}")
        print(f"OTA_IMAGE_UPLOAD: *** image vers: {vers}")
        print(f"OTA_IMAGE_UPLOAD: *** image type: {platform}-{hardware}")
        print(f"OTA_IMAGE_UPLOAD: *** image name: {filename}")
        print(f"OTA_IMAGE_UPLOAD: *** image path: {cached_path}")
        print(f"OTA_IMAGE_UPLOAD: *** image host: {server}")
        
        if cached_path.exists():
            print(f"OTA_IMAGE_UPLOAD: image {filename} already exists in build directory. No action taken.")
            return
        
        upload_successful = upload_image(firmware_path, server, filename)
        if upload_successful:
            try:
                import shutil
                shutil.copy2(firmware_path, cached_path)
                print(f"OTA_IMAGE_UPLOAD: image copied to build directory: {cached_path}")
            except Exception as e:
                print(f"OTA_IMAGE_UPLOAD: error -- failed to copy image to build directory: {str(e)}")
    
    except Exception as e:
        print(f"OTA_IMAGE_UPLOAD: critical error -- {str(e)}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", on_built)