import json
import sys
import os

def update_version_in_files(version_string, file_paths):
    for file_path in file_paths:
        if not os.path.isfile(file_path):
            print(f"File not found: {file_path}")
            continue
            
        # Read the JSON file
        with open(file_path, 'r') as file:
            try:
                vcpkg_data = json.load(file)
            except json.JSONDecodeError as e:
                print(f"Error decoding JSON from {file_path}: {e}")
                continue
        
        # Update the "version-string" property
        vcpkg_data["version-string"] = version_string
        
        # Save the updated JSON back to the file
        with open(file_path, 'w') as file:
            json.dump(vcpkg_data, file, indent=2)
        
        print(f"Updated {file_path} with version-string: {version_string}")

def get_file_paths(file_names):
    script_path = os.path.abspath(__file__)
    script_dir = os.path.dirname(script_path)
    file_paths = [os.path.join(script_dir, "..", file_name) for file_name in file_names]
    return file_paths


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python update-version.py <version_string>")
        sys.exit(1)

    version_string = sys.argv[1]

    json_files = [
        "vcpkg.json",
        "vcpkg-no-ssl.json",
        "vcpkg-with-ssl.json"
    ]

    update_version_in_files(version_string, get_file_paths(json_files))