import json
import urllib.request

URL = "https://www.loldrivers.io/api/drivers.json"


def as_str(x):
    if x is None:
        return ""
    if isinstance(x, str):
        return x
    if isinstance(x, list):
        return " | ".join(
            " ".join(str(v) for v in i.values() if v is not None)
            if isinstance(i, dict)
            else str(i)
            for i in x
        )
    if isinstance(x, dict):
        return " ".join(str(v) for v in x.values() if v is not None)
    return str(x)


def main():
    data = json.load(urllib.request.urlopen(URL, timeout=120))
    rows = []
    for e in data:
        if (e.get("Category") or "").lower() not in ("vulnerable driver", "vulnerable"):
            continue
        for s in e.get("KnownVulnerableSamples") or []:
            sig = as_str(s.get("Signature"))
            if (
                "Hardware Compatibility Publisher" not in sig
                and "Microsoft Corporation" not in sig
            ):
                continue
            fn = s.get("Filename") or s.get("OriginalFilename") or ""
            if not fn or len(fn) < 4:
                continue
            rows.append(
                {
                    "file": fn,
                    "sha256": s.get("SHA256"),
                    "company": as_str(s.get("Company"))[:60],
                    "product": as_str(s.get("Product"))[:60],
                    "sig": sig[:200],
                    "hvci": s.get("LoadsDespiteHVCI"),
                    "ver": e.get("Verified"),
                    "tags": e.get("Tags"),
                    "ref": (e.get("Resources") or [""])[0],
                    "desc": as_str((e.get("Commands") or {}).get("Description"))[:180],
                }
            )

    seen = set()
    uniq = []
    for r in rows:
        if r["sha256"] in seen:
            continue
        seen.add(r["sha256"])
        uniq.append(r)

    uniq.sort(key=lambda r: (r["hvci"] is not True, r["file"].lower()))
    print("named MS/WHQL vulnerable samples:", len(uniq))
    for r in uniq[:45]:
        print(
            f"{r['file']:32} HVCI={str(r['hvci']):5} "
            f"{r['company'][:26]:26} {r['sha256'][:18]}..."
        )
        print(f"  sig: {r['sig'][:150]}")
        if r["ref"]:
            print(f"  ref: {r['ref']}")

    out = r"c:\Users\jhern\VulnDriver\DrvEye\results\named_ms_kernel_vuln.json"
    with open(out, "w", encoding="utf-8") as f:
        json.dump(uniq, f, indent=2)
    print("Wrote", out, "count", len(uniq))


if __name__ == "__main__":
    main()
