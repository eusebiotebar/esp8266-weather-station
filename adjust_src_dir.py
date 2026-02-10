import os
import glob
import shutil

Import("env")
env['PROJECT_SRC_DIR'] = env['PROJECT_DIR'] + os.sep + "examples" + os.sep + env["PIOENV"]
print("Setting the project directory to: {}".format(env['PROJECT_SRC_DIR']))

# Add src directory to include path
src_dir = os.path.join(env['PROJECT_DIR'], 'src')
env.Append(CPPPATH=[src_dir])
print("Added src to CPPPATH: {}".format(src_dir))

# Copy all .cpp files from src to the example directory so they get compiled
# This is the simplest approach that works with PlatformIO
example_dir = env['PROJECT_SRC_DIR']
cpp_files = glob.glob(os.path.join(src_dir, '*.cpp'))

for cpp_file in cpp_files:
    dest_file = os.path.join(example_dir, os.path.basename(cpp_file))
    if not os.path.exists(dest_file):
        shutil.copy2(cpp_file, dest_file)
        print("Copied: {} to example directory".format(os.path.basename(cpp_file)))















