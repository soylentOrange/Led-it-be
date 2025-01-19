import gzip
import os
import sys

os.makedirs('.pio/assets', exist_ok=True)
for filename in ['favicon-32x32.png', 'favicon-96x96.png', 'favicon.svg', 'logo_generic_safeboot.svg', 'hotspot-detect.html']:
    skip = False
    if os.path.isfile('.pio/assets/' + filename + '.timestamp'):
        with open('.pio/assets/' + filename + '.timestamp', 'r', -1, 'utf-8') as timestampFile:
            if os.path.getmtime('assets/' + filename) == float(timestampFile.readline()):
                skip = True
    if skip:
        sys.stderr.write(f"assets.py: {filename} up to date\n")
        continue
    with open('assets/' + filename, 'rb') as inputFile:
        with gzip.open('.pio/assets/' + filename + '.gz', 'wb') as outputFile:
            sys.stderr.write(f"assets.py: gzip \'assets/{filename}\' to \'.pio/assets/{filename}.gz\'\n")
            outputFile.writelines(inputFile)
    with open('.pio/assets/' + filename + '.timestamp', 'w', -1, 'utf-8') as timestampFile:
        timestampFile.write(str(os.path.getmtime('assets/' + filename)))