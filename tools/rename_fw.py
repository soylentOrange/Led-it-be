""" 
    EMS-ESP - https://github.com/emsesp/EMS-ESP
    Copyright 2020-2023  Paul Derbyshire, 2024 Robert Wendlandt

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. 

"""

import sys
import subprocess
import shutil
import re
import os
Import("env")
import hashlib


OUTPUT_DIR = "build{}".format(os.path.sep)

def readFlag(flag):
    buildFlags = env.ParseFlags(env["BUILD_FLAGS"])
    # print(buildFlags.get("CPPDEFINES"))
    for define in buildFlags.get("CPPDEFINES"):
        if (define == flag or (isinstance(define, list) and define[0] == flag)):
            # print("Found "+flag+" = "+define[1])
            # strip quotes ("") from define[1]
            cleanedFlag = re.sub(r'^"|"$', '', define[1])
            return cleanedFlag
    return None

def getVersion():
    # hash
    ret = subprocess.run(
        ["git", "rev-parse", "HEAD"], stdout=subprocess.PIPE, text=True, check=False
    )  # Uses any tags
    full_hash = ret.stdout.strip()
    short_hash = full_hash[:7]

    # branch
    ref_name = os.environ.get("REF_NAME")
    if ref_name:
        branch = ref_name
    else:
        ret = subprocess.run(
            ["git", "symbolic-ref", "--short", "HEAD"],
            stdout=subprocess.PIPE,
            text=True,
            check=False,
        )  # retrieve branch name
        branch = ret.stdout.strip()
        branch = branch.replace("/", "")
        branch = branch.replace("-", "")
        branch = branch.replace("_", "")

    if branch == "":
        raise Exception("No branch name found")

    # is_tag ?
    tagPattern = re.compile("^v[0-9]+.[0-9]+.[0-9]+([_-][a-zA-Z0-9]+)?$")
    is_tag = branch.startswith("v") and len(branch) >= 6 and tagPattern.match(branch)

    version = branch
    if not is_tag:
        version += "_" + short_hash

    # local modifications ?
    has_local_modifications = False
    if not ref_name:
        # Check if the source has been modified since the last commit
        ret = subprocess.run(
            ["git", "diff-index", "--quiet", "HEAD", "--"],
            stdout=subprocess.PIPE,
            text=True,
            check=False,
        )
        has_local_modifications = ret.returncode != 0

    if has_local_modifications:
        version += "_modified"

    return version


def bin_copy(source, target, env):

    # get the build info
    app_version = getVersion()
    app_name = readFlag("APP_NAME")
    build_target = env.get('PIOENV')
    board = env.GetProjectOption('board')
    
    # print information's
    print("App Version: " + app_version)
    print("App Name: " + app_name)
    print("Build Target: " + build_target)
    print("Board: " + board)

    # convert . to - so Windows doesn't complain
    variant = app_name + "_" +  board + "_" + app_version.replace(".", "-")

    # check if output directories exist and create if necessary
    if not os.path.isdir(OUTPUT_DIR):
        os.mkdir(OUTPUT_DIR)

    for d in ['firmware']:
        if not os.path.isdir("{}{}".format(OUTPUT_DIR, d)):
            os.mkdir("{}{}".format(OUTPUT_DIR, d))

    # create string with location and file names based on variant
    bin_file = "{}firmware{}{}.bin".format(OUTPUT_DIR, os.path.sep, variant)
    md5_file = "{}firmware{}{}.md5".format(OUTPUT_DIR, os.path.sep, variant)
    factory_bin_file = "{}firmware{}{}.factory.bin".format(OUTPUT_DIR, os.path.sep, variant)
    factory_md5_file = "{}firmware{}{}.factory.md5".format(OUTPUT_DIR, os.path.sep, variant)

    # check if new target files exist and remove if necessary
    for f in [bin_file]:
        if os.path.isfile(f):
            os.remove(f)

    # check if new target files exist and remove if necessary
    for f in [md5_file]:
        if os.path.isfile(f):
            os.remove(f)
    
    # check if new target files exist and remove if necessary
    for f in [factory_bin_file]:
        if os.path.isfile(f):
            os.remove(f)

    # check if new target files exist and remove if necessary
    for f in [factory_md5_file]:
        if os.path.isfile(f):
            os.remove(f)

    print("Renaming firmware.bin to "+bin_file)
    print("Renaming firmware.factory.bin to "+factory_bin_file)

    # copy firmware.bin to firmware/<variant>.bin
    shutil.copy(str(target[0]), bin_file)
    # copy firmware.factory.bin to firmware/<variant>.bin
    shutil.copy(str(target[0]), factory_bin_file)

    with open(bin_file,"rb") as f:
        result = hashlib.md5(f.read())
        print("Calculating MD5: "+result.hexdigest())
        file1 = open(md5_file, 'w')
        file1.write(result.hexdigest())
        file1.close()

    with open(factory_bin_file,"rb") as f:
        result = hashlib.md5(f.read())
        print("Calculating MD5: "+result.hexdigest())
        file2 = open(factory_md5_file, 'w')
        file2.write(result.hexdigest())
        file2.close()

env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", [bin_copy])
env.AddPostAction("$BUILD_DIR/${PROGNAME}.md5", [bin_copy])
