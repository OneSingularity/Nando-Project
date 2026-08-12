import re
import urllib.request

url = "https://www.loldrivers.io/drivers/ff74f03e-e4ce-4242-bfe3-60601056bb34/"
html = urllib.request.urlopen(
    urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"}), timeout=30
).read().decode("utf-8", "replace")
for m in re.findall(r'href=["\']([^"\']+)["\']', html):
    low = m.lower()
    if any(x in low for x in ["download", ".sys", ".bin", "github", "drivers/", "raw", "blob"]):
        print(m)
print("---")
for m in re.finditer(r".{0,100}download.{0,150}", html, re.I):
    print(re.sub(r"\s+", " ", m.group(0))[:250])
