#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Fill in the localized Pokemon/move names DEX_NAMES and MOVE_NAMES never
got past what was hand-translated once.

  python3 tools/gen_dex_names.py

DEX_NAMES (dex.h) is hand-written, NOT part of gen_dex.py's output, and was
only ever filled in up to dex 386 (Kanto..Hoenn) -- everything from Sinnoh
on (387..DEX_COUNT) is nullptr in every language, so dexName() falls back to
the English canonical name regardless of the language picked. Of the six
rows that DO exist for 1..386, only FR and DE hold real translations; ES,
EN, IT and PT all mirror the English canonical name there too -- that is
the existing, accepted state (EN correctly so; ES/IT/PT were apparently
never sourced), and this script does NOT touch it. It only APPENDS entries
for dex 387..DEX_COUNT, for ES/EN/FR/DE/IT (PT is asked for too, but PokeAPI
essentially never has an official Portuguese species name, so those come
back empty almost every time and fall back exactly like a species with no
sprite falls back to its dex number -- not a bug to "fix" by inventing one).

MOVE_NAMES (moves.h) has the same shape: hand-written, only the FR row
filled in ("verified against Bulbapedia's List of moves in other languages
move by move", per its own comment -- left untouched here). This script
fills ES/DE/IT (EN is moot: MOVE_TBL's own name already IS the English
name, so moveName()'s fallback already gives the right answer there).

Both tables need every name stripped of accents -- see CLAUDE.md's hard
rule: the bitmap font has no glyphs for anything outside plain ASCII, which
is why the existing FR/DE rows already read "CHARGE"/"KRAFT" and not
"Charge"/"Kraft" with real capitals and no diacritics to begin with.

Responses are cached under tools/pokeapi_names_cache/ (gitignored, separate
from fetch_pokeapi.py's own tools/pokeapi_cache/ so the two scripts never
fight over the same files). Delete the directory to force a refetch.

This does NOT rewrite dex.h/moves.h in place -- printing the two blocks and
splicing them in by hand keeps a human in the loop for exactly the kind of
"does FARFETCHD-style stripping look right" judgment call that a fully
automatic in-place edit would paper over.
"""
import json
import os
import re
import ssl
import sys
import time
import unicodedata
import urllib.request

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from dex_data import DEX
from dex_moves import MOVES

try:
    import certifi
    SSL_CTX = ssl.create_default_context(cafile=certifi.where())
except ImportError:
    SSL_CTX = None

HERE = os.path.dirname(os.path.abspath(__file__))
CACHE = os.path.join(HERE, 'pokeapi_names_cache')
LANGS = ['es', 'en', 'fr', 'de', 'it']  # DEX_LANG_COUNT order minus pt, added separately
DEX_LANG_ORDER = ['es', 'en', 'fr', 'de', 'it', 'pt']  # matches LANG_CODES in TamaPoke.ino

DEX_COUNT = max(d[0] for d in DEX)
SLUG_BY_DEX = {d[0]: d[1] for d in DEX}
# dex_data.py's slugs drop hyphens PokeAPI's species endpoint actually needs
# (its OWN slug for the same species, e.g. dex_data.py's "tapukoko" 404s
# where PokeAPI wants "tapu-koko"). Only affects the lookup URL, not any
# output -- the sanitizer only ever sees the returned display name.
SLUG_OVERRIDE = {
    439: 'mime-jr', 474: 'porygon-z', 772: 'type-null',
    782: 'jangmo-o', 783: 'hakamo-o', 784: 'kommo-o',
    785: 'tapu-koko', 786: 'tapu-lele', 787: 'tapu-bulu', 788: 'tapu-fini',
}
for _dex, _slug in SLUG_OVERRIDE.items():
    SLUG_BY_DEX[_dex] = _slug


def fetch_json(url, cache_key):
    path = os.path.join(CACHE, cache_key + '.json')
    if os.path.exists(path):
        with open(path, encoding='utf-8') as f:
            return json.load(f)
    os.makedirs(CACHE, exist_ok=True)
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    with urllib.request.urlopen(req, timeout=30, context=SSL_CTX) as r:
        data = json.load(r)
    with open(path, 'w', encoding='utf-8') as f:
        json.dump(data, f)
    time.sleep(0.1)  # be polite to a free API
    return data


# Apostrophes/periods/hyphens: DEX_NAMES 1..386 strips them outright
# (FARFETCHD, not FARFETCH'D) except where a period is kept as "X. Y"
# (MR. MIME) and a literal hyphen-as-dash becomes an underscore (HO_OH).
# There is no single mechanical rule that reproduces every one of those by
# formula -- they were judgment calls -- so this only handles the GENERAL
# case (strip diacritics, uppercase, drop apostrophes, collapse whatever is
# left to spaces) and leaves anything that looks like MR./HO-OH/a Farfetch'd
# apostrophe for the human splicing this in to sanity-check against the
# existing 1..386 convention.
def sanitize(name):
    # NFKD splits accented letters into base+combining mark; keeping only
    # the base ASCII drops every accent (e.g. "Florizarré" -> "Florizarre").
    decomposed = unicodedata.normalize('NFKD', name)
    ascii_only = decomposed.encode('ascii', 'ignore').decode('ascii')
    ascii_only = ascii_only.replace("'", '').replace('’', '')
    ascii_only = re.sub(r'[^A-Za-z0-9. -]', ' ', ascii_only)
    ascii_only = re.sub(r'\s+', ' ', ascii_only).strip()
    return ascii_only.upper()


def species_names(dex_num):
    slug = SLUG_BY_DEX[dex_num]
    data = fetch_json('https://pokeapi.co/api/v2/pokemon-species/%s' % slug,
                       'species_%03d_%s' % (dex_num, slug))
    by_lang = {}
    for n in data.get('names', []):
        lang = n['language']['name']
        if lang in DEX_LANG_ORDER:
            by_lang[lang] = sanitize(n['name'])
    return by_lang


def move_names(slug):
    if not slug:  # STRUGGLE has no slug in dex_moves.py but is a real move
        slug = 'struggle'
    data = fetch_json('https://pokeapi.co/api/v2/move/%s' % slug, 'move_%s' % slug)
    by_lang = {}
    for n in data.get('names', []):
        lang = n['language']['name']
        if lang in DEX_LANG_ORDER:
            by_lang[lang] = sanitize(n['name'])
    return by_lang


def emit_dex_block():
    print('=== DEX_NAMES: append these to each language row, dex 387..%d ===' % DEX_COUNT)
    per_lang = {l: [] for l in DEX_LANG_ORDER}
    missing = {l: [] for l in DEX_LANG_ORDER}
    first_species_range = min(d for d in SLUG_BY_DEX if d > 386)
    for dex_num in range(first_species_range, DEX_COUNT + 1):
        by_lang = species_names(dex_num)
        for lang in DEX_LANG_ORDER:
            name = by_lang.get(lang)
            per_lang[lang].append(name)
            if not name:
                missing[lang].append(dex_num)
    for lang in DEX_LANG_ORDER:
        print('\n-- %s --' % lang.upper())
        print(', '.join('"%s"' % n if n else 'nullptr' for n in per_lang[lang]))
    print('\n=== missing counts (fell back to nullptr -> canonical name) ===')
    for lang in DEX_LANG_ORDER:
        print('%s: %d missing of %d' % (lang, len(missing[lang]), DEX_COUNT - 386))
    return per_lang, missing


def emit_move_block():
    print('\n\n=== MOVE_NAMES: append these to the ES/DE/IT rows (index order = MOVE_TBL) ===')
    wanted_langs = ['es', 'de', 'it']
    per_lang = {l: [] for l in wanted_langs}
    missing = {l: [] for l in wanted_langs}
    for name, slug, *_ in MOVES:
        by_lang = move_names(slug)
        for lang in wanted_langs:
            n = by_lang.get(lang)
            per_lang[lang].append(n)
            if not n:
                missing[lang].append(name)
    for lang in wanted_langs:
        print('\n-- %s --' % lang.upper())
        print(', '.join('"%s"' % n if n else 'nullptr' for n in per_lang[lang]))
    print('\n=== missing counts ===')
    for lang in wanted_langs:
        print('%s: %d missing of %d' % (lang, len(missing[lang]), len(MOVES)))
    return per_lang, missing


if __name__ == '__main__':
    emit_dex_block()
    emit_move_block()
