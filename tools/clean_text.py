"""Clean the OCR text table: data/text_raw.tsv -> data/text.tsv.

Repairs common misreads ("!" read as "l") and drops entries that are
recognition garbage from decorative lettering. Words are judged against the
vocabulary of the whole table: real words appear in many entries.
Usage: python clean_text.py [show]
"""
import re, collections, random, sys, os

HERE = os.path.dirname(os.path.abspath(__file__))
RAW = os.path.join(HERE, "..", "data", "text_raw.tsv")
OUT = os.path.join(HERE, "..", "data", "text.tsv")

rows = [l.rstrip("\n").split("\t") for l in open(RAW, encoding="utf-8") if not l.startswith("#")]
cnt = collections.Counter()
for t in set(r[2] for r in rows):
    for w in set(re.findall(r"[A-Za-z]+", t.lower())):
        cnt[w] += 1

def fix(t):
    t = re.sub("[•�]+", "...", t)   # OCR reads an ellipsis as bullets
    t = re.sub(r"Sparking[dlIU!]+", "Sparking!", t)
    t = re.sub(r"\b[Il] (Ki [Gg]auge)", r"1 \1", t)          # "I Ki gauge" is "1 Ki gauge"
    t = re.sub(r"(\w)' s\b", r"\1's", t)                     # "Goku' s" is "Goku's"
    t = re.sub(r"\b(Goku|Vegeta|Ha)l!", r"\1!!", t)             # "Gokul!" is "Goku!!"
    t = t.replace("I-J", "U").replace("IJ", "U")              # the letter U is read as "IJ" or "I-J"
    t = re.sub(r"^G \+11 during battle$", "Guard, down, down, during battle", t)   # item skill cards: G + down down
    t = t.replace("G+I I", "Guard, down, down")
    def bang(m):
        base, tail = m.group(1), m.group(2)
        if cnt[(base + tail).lower()] >= 2 or cnt[base.lower()] < 2:
            return m.group(0)
        return base + "!" * len(tail)
    return re.sub(r"\b([A-Za-z]{3,}?)([lI]{1,3})(?=[.!?]?$|[.!?]?\s)", bang, t)

ALLOWED = re.compile(r"[^A-Za-z0-9 .,!?'\"\-:;()/%&+~*#\[\]<>=…]")

def ok(t):
    if ALLOWED.search(t):
        return False
    ws = re.findall(r"[A-Za-z]+", t)
    if not ws:
        return False
    known = [w for w in ws if cnt[w.lower()] >= 2]
    if not known and not (len(ws) <= 3 and all(re.fullmatch(r"[A-Z][a-z]{2,}", w) for w in ws)):
        return False
    return len(known) >= 0.5 * len(ws) or len(ws) <= 3

ROSTER = ["Goku", "Kid Goku", "Kid Gohan", "Teen Gohan", "Gohan", "Great Saiyaman", "Goten", "Trunks",
          "Kid Trunks", "Vegeta", "Piccolo", "Krillin", "Yamcha", "Tien", "Uub", "Videl", "Hercule", "Supreme Kai",
          "Raditz", "Nappa", "Saibamen", "Captain Ginyu", "Recoome", "Frieza", "Cooler", "Android 16", "Android 17",
          "Android 18", "Dr. Gero", "Cell", "Cell Jr.", "Majin Buu", "Super Buu", "Kid Buu", "Dabura", "Broly",
          "Bardock", "Omega Shenron", "Gotenks", "Gogeta", "Vegito", "Super Saiyan"]
import difflib
LOWER = {n.lower(): n for n in ROSTER}
def roster(t):
    """Short texts that are a misread fighter name become the name."""
    if re.search(r"\d", t):
        return t
    core = re.sub(r"^[.\s]+|[.!?\s]+$", "", t).replace("#", "")
    if not core or core.lower() in LOWER or len(core) > 18:
        return t
    m = difflib.get_close_matches(core.lower(), list(LOWER), n=1, cutoff=0.75)
    if not m or len(m[0].split()) != len(core.split()):
        return t
    out = LOWER[m[0]] + t[len(t.rstrip("!?.")):]
    return out.replace("..", ".") if LOWER[m[0]].endswith(".") else out

rows = [[r[0], r[1], roster(fix(r[2]))] for r in rows]
keep = [r for r in rows if ok(r[2])]
FIX = os.path.join(HERE, "..", "data", "fixes.tsv")
fixes = {}
for l in open(FIX, encoding="utf-8"):
    if l.startswith("#") or not l.strip("\n"): continue
    p = l.rstrip("\n").split("\t")
    fixes[(p[0], p[1])] = p[2] if len(p) > 2 else ""
keep = [r for r in keep if (r[0], r[1]) not in fixes]
keep += [[k[0], k[1], t] for k, t in fixes.items() if t]
keep.sort(key=lambda r: (r[0], int(r[1])))
drop = [r for r in rows if not ok(r[2])]
print(len(rows), "entries,", len(keep), "kept,", len(drop), "dropped")
if "show" in sys.argv:
    random.seed(7)
    for r in random.sample(drop, min(40, len(drop))):
        print("  dropped:", r[2][:90])
with open(OUT, "w", encoding="utf-8") as o:
    o.write("# pack checksum, texture index, text (OCR of the game's text images, cleaned by tools/clean_text.py)\n")
    for r in keep:
        o.write("\t".join(r) + "\n")
