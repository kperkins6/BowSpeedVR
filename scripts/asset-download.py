import hashlib
import pathlib
import sys
import urllib.request

url, expected, destination = sys.argv[1:]
if not url.startswith('https://'):
    raise SystemExit('Only HTTPS assets supported')
temporary = pathlib.Path(destination + '.python-part')
try:
    urllib.request.urlretrieve(url, temporary)
    actual = hashlib.file_digest(temporary.open('rb'), 'sha512').hexdigest()
    if actual.lower() != expected.lower():
        raise ValueError('SHA512 mismatch')
    temporary.replace(destination)
finally:
    temporary.unlink(missing_ok=True)
