import json
import re
import urllib.request
from collections import Counter

URL = "https://www.loldrivers.io/api/drivers.json"


def as_str(x):
    if x is None:
        return ""
    if isinstance(x, str):
        return x
    if isinstance(x, list):
        parts = []
        for i in x:
            if isinstance(i, dict):
                parts.append(" ".join(str(v) for v in i.values() if v is not None))
            else:
                parts.append(str(i))
        return " | ".join(parts)
    if isinstance(x, dict):
        return " ".join(str(v) for v in x.values() if v is not None)
    return str(x)


def main():
    data = json.load(urllib.request.urlopen(URL, timeout=120))
    whql = []
    cats = Counter()
    companies = Counter()

    for e in data:
        for s in e.get("KnownVulnerableSamples") or []:
            blob = " ".join(
                [
                    as_str(s.get("Signature")),
                    as_str(s.get("Publisher")),
                    as_str(s.get("Signatures")),
                ]
            )
            if "Hardware Compatibility" not in blob:
                continue
            cats[e.get("Category")] += 1
            companies[as_str(s.get("Company"))[:60]] += 1
            whql.append(
                {
                    "category": e.get("Category"),
                    "verified": e.get("Verified"),
                    "tags": e.get("Tags"),
                    "filename": s.get("Filename") or s.get("OriginalFilename"),
                    "sha256": s.get("SHA256"),
                    "company": as_str(s.get("Company"))[:100],
                    "product": as_str(s.get("Product"))[:100],
                    "signature": as_str(s.get("Signature"))[:220],
                    "hvci": s.get("LoadsDespiteHVCI"),
                    "desc": as_str((e.get("Commands") or {}).get("Description"))[:220],
                    "usecase": as_str((e.get("Commands") or {}).get("Usecase"))[:100],
                    "resources": (e.get("Resources") or [])[:2],
                }
            )

    print("WHQL / Hardware Compatibility samples:", len(whql))
    print("categories:", dict(cats))
    print("top companies:", companies.most_common(20))

    seen = set()
    uniq = []
    for h in whql:
        sh = h["sha256"]
        if not sh or sh in seen:
            continue
        seen.add(sh)
        uniq.append(h)

    print("unique SHA256:", len(uniq))
    uniq.sort(
        key=lambda h: (
            (h["category"] or "").lower() != "vulnerable",
            str(h["verified"]).upper() != "TRUE",
            h["filename"] or "",
        )
    )

    for h in uniq[:35]:
        print("---")
        print(
            f"{h['filename']}  cat={h['category']}  ver={h['verified']}  HVCI={h['hvci']}"
        )
        print(f"  sha256={h['sha256']}")
        print(f"  company={h['company']}  product={h['product']}")
        print(f"  sig={h['signature'][:180]}")
        print(f"  use={h['usecase']}")
        print(f"  desc={h['desc'][:160]}")
        if h["resources"]:
            print(f"  ref={h['resources'][0]}")

    out = r"c:\Users\jhern\VulnDriver\DrvEye\results\ms_kernel_vuln_drivers.json"
    with open(out, "w", encoding="utf-8") as f:
        json.dump(uniq[:50], f, indent=2)
    print("Wrote", out)

    # Also show Microsoft Corporation signed (inbox-ish / MS-signed)
    print("\n=== Signature contains Microsoft Corporation (non-WHQL label) ===")
    ms = []
    for e in data:
        for s in e.get("KnownVulnerableSamples") or []:
            blob = as_str(s.get("Signature")) + " " + as_str(s.get("Signatures"))
            if re.search(r"Microsoft Corporation", blob) and "Hardware Compatibility" not in blob:
                ms.append(
                    {
                        "category": e.get("Category"),
                        "filename": s.get("Filename") or s.get("OriginalFilename"),
                        "sha256": s.get("SHA256"),
                        "signature": as_str(s.get("Signature"))[:160],
                        "company": as_str(s.get("Company"))[:80],
                        "verified": e.get("Verified"),
                        "hvci": s.get("LoadsDespiteHVCI"),
                    }
                )
    seen = set()
    for h in ms:
        if h["sha256"] in seen:
            continue
        seen.add(h["sha256"])
        print(
            h["filename"],
            h["category"],
            "ver=",
            h["verified"],
            "HVCI=",
            h["hvci"],
            h["sha256"][:20],
            h["company"],
        )
        if len(seen) >= 20:
            break


if __name__ == "__main__":
    main()
