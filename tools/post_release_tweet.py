#!/usr/bin/env python3
"""
Replicant303 - post a release announcement to X (Twitter).

X's v2 write endpoints reject OAuth 2.0 app-only tokens: POST /2/tweets only
accepts OAuth 1.0a User Context (or an OAuth2 user-context token minted through
a PKCE flow). A bearer token copied from the developer portal is app-only and
returns 403, so this script signs the request itself with OAuth 1.0a.

Credentials come from the environment and are never read from disk:

    X_API_KEY             consumer key
    X_API_SECRET          consumer secret
    X_ACCESS_TOKEN        access token for the posting account
    X_ACCESS_SECRET       access token secret

The request is signed per RFC 5849: percent-encode, sort, join the parameter
string, build "METHOD&url&params", HMAC-SHA1 with "secret&token_secret",
base64. Only the standard library is used, so it runs on a bare runner.

Usage:
    X_API_KEY=... X_API_SECRET=... X_ACCESS_TOKEN=... X_ACCESS_SECRET=... \\
        python3 tools/post_release_tweet.py "the tweet text"

    --dry-run prints the text and the authorization header shape without
    sending anything. Use it to check formatting before a real post.
"""

import base64
import hashlib
import hmac
import json
import os
import secrets
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

ENDPOINT = "https://api.x.com/2/tweets"
MAX_TWEET_CHARS = 280
# X bills every link as this many characters regardless of real length.
URL_WEIGHT = 23


def _weighted_length(text: str) -> int:
    """Length as X counts it: each URL replaced by URL_WEIGHT chars."""
    import re
    total = 0
    last = 0
    for match in re.finditer(r"https?://\S+", text):
        total += len(text[last:match.start()]) + URL_WEIGHT
        last = match.end()
    return total + len(text[last:])


def _pct(value: str) -> str:
    """RFC 3986 percent-encoding. quote() already leaves -._~ alone."""
    return urllib.parse.quote(value, safe="")


def build_authorization(method: str, url: str, params: dict, consumer_secret: str,
                        token_secret: str) -> str:
    """Return the OAuth header for a request with no query string or body."""
    oauth = dict(params)
    param_string = "&".join(f"{_pct(k)}={_pct(v)}" for k, v in sorted(oauth.items()))

    base_string = "&".join([method.upper(), _pct(url), _pct(param_string)])
    signing_key = f"{_pct(consumer_secret)}&{_pct(token_secret)}"

    signature = base64.b64encode(
        hmac.new(signing_key.encode(), base_string.encode(), hashlib.sha1).digest()
    ).decode()
    oauth["oauth_signature"] = signature

    return "OAuth " + ", ".join(
        f'{_pct(k)}="{_pct(v)}"' for k, v in sorted(oauth.items())
    )


def post_tweet(text: str, payload_auth: dict) -> dict:
    body = json.dumps({"text": text}).encode()

    params = {
        "oauth_consumer_key": payload_auth["key"],
        "oauth_nonce": secrets.token_hex(16),
        "oauth_signature_method": "HMAC-SHA1",
        "oauth_timestamp": str(int(time.time())),
        "oauth_token": payload_auth["token"],
        "oauth_version": "1.0a",
    }

    # A JSON body is not part of the signature base string: OAuth 1.0a signs
    # only the request line, the query string, and form-encoded bodies.
    header = build_authorization(
        "POST", ENDPOINT, params,
        payload_auth["secret"], payload_auth["token_secret"],
    )

    request = urllib.request.Request(
        ENDPOINT,
        data=body,
        headers={"Authorization": header, "Content-Type": "application/json"},
        method="POST",
    )

    with urllib.request.urlopen(request, timeout=30) as response:
        return json.loads(response.read().decode())


def main(argv: list) -> int:
    args = [a for a in argv[1:] if a != "--dry-run"]
    dry_run = "--dry-run" in argv[1:]

    if not args:
        print("usage: post_release_tweet.py [--dry-run] \"tweet text\"", file=sys.stderr)
        return 2

    text = args[0]

    if _weighted_length(text) > MAX_TWEET_CHARS:
        print(
            f"error: tweet is {_weighted_length(text)} weighted chars "
            f"(X counts each link as {URL_WEIGHT}), allows {MAX_TWEET_CHARS}",
            file=sys.stderr,
        )
        return 2

    creds = {
        "key": os.environ.get("X_API_KEY", ""),
        "secret": os.environ.get("X_API_SECRET", ""),
        "token": os.environ.get("X_ACCESS_TOKEN", ""),
        "token_secret": os.environ.get("X_ACCESS_SECRET", ""),
    }

    missing = [f"X_{name.upper()}" for name, value in
               (("api_key", creds["key"]), ("api_secret", creds["secret"]),
                ("access_token", creds["token"]), ("access_secret", creds["token_secret"]))
               if not value]

    if missing:
        print(f"error: missing credentials: {', '.join(missing)}", file=sys.stderr)
        return 2

    if dry_run:
        header = build_authorization(
            "POST", ENDPOINT,
            {"oauth_consumer_key": creds["key"], "oauth_nonce": "x" * 32,
             "oauth_signature_method": "HMAC-SHA1", "oauth_timestamp": "0",
             "oauth_token": creds["token"], "oauth_version": "1.0a"},
            creds["secret"], creds["token_secret"],
        )
        print(f"[dry-run] text ({len(text)} chars):\n{text}\n")
        print("[dry-run] authorization header shape:")
        print("  " + ", ".join(p.split("=")[0] for p in header[len("OAuth "):].split(", ")))
        return 0

    try:
        result = post_tweet(text, creds)
    except urllib.error.HTTPError as error:
        detail = error.read().decode(errors="replace")
        print(f"error: X returned HTTP {error.code}\n{detail}", file=sys.stderr)
        return 1

    tweet_id = result.get("data", {}).get("id", "?")
    print(f"posted tweet {tweet_id}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
