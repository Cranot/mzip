#!/usr/bin/env python3
"""A plain JSON fetch for repo FILES.

The api() helper in kquant_given returns None for resolve/main/*.json URLs -- it is built for the
/api/ endpoints. Every caller that used it to read config.json or model.safetensors.index.json got a
silent None and carried on with a default, which is how the Q/K permutation test came to be disabled
on every run without a single error line."""
import json, os, urllib.request
TOK = os.environ.get("HF_TOKEN", "")

def get_json(u, timeout=45):
    r = urllib.request.Request(u)
    if TOK: r.add_header("Authorization", f"Bearer {TOK}")
    for _ in range(3):
        try:
            with urllib.request.urlopen(r, timeout=timeout) as f:
                return json.loads(f.read())
        except Exception:
            pass
    return None
