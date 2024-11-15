import os
import subprocess
import sys
import time
import signal

def run_command(command, cwd=None):
    """Run a shell command."""
    result = subprocess.run(command, shell=True, cwd=cwd, check=True)
    return result

def clone_or_update_webthing_tester_repo():
    """Clone the repository if it doesn't exist, otherwise update it."""
    
    repo_dir = "webthing-tester"
    branch = "time-regex-option"

    if os.path.isdir(repo_dir):
        print(f"Updating repository in {repo_dir}...")
        run_command(f"git pull origin {branch}", cwd=repo_dir)
    else:
        print(f"Cloning repository to {repo_dir}...")
        run_command(f"git clone -b {branch} https://github.com/bw-hro/webthing-tester.git")

def install_python_dependencies():
    """Install required Python packages."""
    print("Installing Python dependencies...")
    run_command("python3 -m venv webthing-tester-venv")
    run_command(".\\webthing-tester-venv\\Scripts\\activate.bat")
    run_command("pip3 install -r webthing-tester/requirements.txt")

def build_webthing_cpp():
    """Build Webthing-CPP if the binaries do not exist."""
    single_bin = "../../build/examples/Release/single-thing.exe"
    multis_bin = "../../build/examples/Release/multiple-things.exe"

    if not os.path.exists(single_bin) or not os.path.exists(multis_bin):
        print("Building Webthing-CPP with examples...")
        run_command("build.bat clean release", cwd="..")
    else:
        print("Binaries already exist, skipping build.")

def run_example(example_path, time_regex, path_prefix=""):
    """Run the example and test it with webthing-tester."""
    print(f"Running example {example_path}...")
    process = subprocess.Popen([example_path])

    try:
        time.sleep(15)  # Wait for the example to start
        test_command = [
            "python",
            "webthing-tester/test-client.py",
            "--time-regex", time_regex,
            "--debug"
        ]
        if path_prefix:
            test_command.extend(["--path-prefix", path_prefix])

        print(f"Running webthing-tester with {test_command}...")
        run_command(" ".join(test_command))

    finally:
        print(f"Terminating example process {example_path}...")
        process.terminate()
        process.wait()

def main():
    clone_or_update_webthing_tester_repo()
    install_python_dependencies()
    build_webthing_cpp()

    # Configure time regex to match timestamps with milliseconds
    time_regex = r"^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}.\d{3}[+-]\d{2}:\d{2}$"

    single_example_path = "../../build/examples/Release/single-thing.exe"
    run_example(single_example_path, time_regex)

    multiple_example_path = "../../build/examples/Release/multiple-things.exe"
    run_example(multiple_example_path, time_regex, path_prefix="/0")

if __name__ == "__main__":
    main()
