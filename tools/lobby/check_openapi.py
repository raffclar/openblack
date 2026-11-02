#!/usr/bin/env python3
"""Check the JSON shapes that openblack reads and writes against the lobby's OpenAPI document.

The lobby (bwgame-service) keeps its contract in openapi/v1.yaml. Give this tool the path of that file from a
checkout of bwgame-service:

    python tools/lobby/check_openapi.py <bwgame-service>/openapi/v1.yaml

For each call of the lobby client and of the game server, the tool checks that the document has the path and the
method, and that each member that openblack reads in the answer (or writes in the request) is in the document with
the same type. A member that openblack needs must be required in the document. The tool prints one line for each
problem and exits with 1 when it finds a problem.

The table below follows components/lobbyapi/src/LobbyApiJson.cpp. Change both together.
"""

import sys

try:
    import yaml
except ImportError:  # pragma: no cover
    sys.exit("check_openapi: this tool needs PyYAML (pip install pyyaml)")

# A member: (name, type, needed). Type is an OpenAPI type ("string", "integer", "number", "boolean", "object",
# "array"), or a dict of members for an object, or a list with one dict for an array of objects. Needed members must
# be required in the document. A type may be None to check only that the member is there.
S, I, N, B = "string", "integer", "number", "boolean"
ACCOUNT = {"id": (I, True), "name": (S, True)}
CLAN_REF = {"id": (I, True), "name": (S, True), "role": (S, False)}
ROOM_PLAYER = {"account": (ACCOUNT, True), "role": (S, True), "clan": (None, True)}
SERVER = {
    "id": (S, True), "hostname": (S, True), "land": (S, True), "gamemode": (S, True), "locked": (B, True),
    "password": (B, True), "numplayers": (I, True), "maxplayers": (I, True), "gamever": (S, True),
    "infoversion": (I, True), "creatureversion": (I, True), "oosversion": (I, True),
    "players": ([ROOM_PLAYER], True), "url": (S, True), "updated": (S, False), "map": (S, False),
}  # fmt: skip
SESSION = {
    "access_token": (S, True), "access_token_expires": (S, True), "refresh_token": (None, True),
    "refresh_token_expires": (S, False), "email_required": (B, False), "account": (ACCOUNT, True),
}  # fmt: skip

# (method, path, request members or None, answer members or None)
CALLS = [
    ("post", "/api/v1/sessions", {"name": (S, True), "password": (S, True), "cookie": (B, False)}, SESSION),
    ("post", "/api/v1/sessions/refresh", {"refresh_token": (S, False)}, SESSION),
    ("delete", "/api/v1/sessions", None, None),
    ("delete", "/api/v1/sessions/current", None, None),
    ("get", "/api/v1/me", None, {
        "id": (I, True), "name": (S, True), "email": (S, False), "email_verified": (B, False), "points": (N, False),
        "games_ranked": (I, False), "country": (S, False), "area": (S, False), "city": (S, False),
        "clans": ([CLAN_REF], False), "date_joined": (S, False),
    }),
    ("get", "/api/v1/servers", None, {"servers": ([SERVER], True)}),
    ("post", "/api/v1/servers/{server_id}/tickets", {"role": (None, False)},
     {"url": (S, True), "ticket": (S, True), "expires": (S, True), "role": (None, False)}),
    ("post", "/api/v1/chat/channels/{channel}/messages", {"text": (S, True)}, None),
    ("put", "/api/v1/servers/{server_id}/clan", {"clan": (None, True)}, {"clan": (None, True)}),
    ("get", "/api/v1/account-names/{name}", None, {"name": (S, True), "valid": (B, True), "available": (B, True)}),
    ("get", "/api/v1/me/clans", None, {"clans": ([{"id": (I, True), "name": (S, True)}], True)}),
    ("get", "/api/v1/clans/{clan_id}", None, {
        "id": (I, True), "name": (S, True), "points": (N, False), "motto": (S, False), "website": (S, False),
        "leader": (None, False), "members": (None, False),
    }),
    ("get", "/api/v1/clans/{clan_id}/logo", None, None),
    ("get", "/api/v1/clans/{clan_id}/creature", None, {
        "kind": (S, True), "alignment": (None, True), "mind": (None, True), "physique": (None, True),
        "creature_type": (None, True), "name": (S, True),
    }),
    ("put", "/api/v1/clans/{clan_id}/creature", {"alignment": (N, True), "mind": (S, True), "physique": (S, True)},
     None),
    ("get", "/api/v1/maps", None, {"maps": ([{
        "number": (I, True), "name": (S, True), "file": (S, True), "players": (I, True), "sha256": (S, True),
        "checksum": (S, True), "size": (I, True), "url": (S, True),
    }], True)}),
    ("get", "/api/v1/maps/{sha256}", None, None),
    ("get", "/api/v1/rankings/players", None, {"count": (I, True), "players": ([{
        "position": (I, True), "account": (ACCOUNT, True), "points": (N, True), "games_ranked": (I, True),
    }], True)}),
    ("get", "/api/v1/rankings/clans", None, {"count": (I, True), "clans": ([{
        "position": (I, True), "id": (I, True), "name": (S, True), "points": (N, True),
    }], True)}),
    ("get", "/api/v1/me/weather", None, {
        "temperature": (I, True), "visibility": (I, True), "type": (I, True), "city": (S, True), "area": (S, False),
        "country": (S, True),
    }),
    ("post", "/api/v1/me/creatures", None, {"url": (S, True)}),
    ("post", "/api/v1/me/skirmish-pings", None, None),
    # The game server
    ("put", "/api/v1/servers/{server_id}/status", {
        "hostname": (S, True), "land": (S, False), "gamemode": (S, True), "locked": (B, False),
        "password": (B, False), "maxplayers": (I, True), "gamever": (S, False),
        "players": ([{"account": (I, True), "role": (S, True), "clan": (None, False)}], False),
    }, {"next_status_in": (I, True), "listed": (B, True), "clans": ([{"account": (I, True), "clan": (I, True)}], True)}),
    ("get", "/api/v1/servers/{server_id}/bans", None,
     {"bans": ([{"account": (ACCOUNT, True), "reason": (S, False)}], True)}),
    ("put", "/api/v1/servers/{server_id}/map", None, None),
]  # fmt: skip

# Calls of openblack that the document does not have yet. They are listed, not counted as problems.
NOT_IN_CONTRACT = [
    ("get", "/api/v1/players/{player_id}"),
    ("get", "/api/v1/players/{player_id}/details"),
]


class Checker:
    def __init__(self, document):
        self.document = document
        self.schemas = document.get("components", {}).get("schemas", {})
        self.problems = []

    def resolve(self, node):
        while isinstance(node, dict) and "$ref" in node:
            node = self.schemas[node["$ref"].split("/")[-1]]
        return node

    def merged(self, node):
        """An object schema with the members and the required list of its allOf parts."""
        node = self.resolve(node)
        properties = dict(node.get("properties") or {})
        required = set(node.get("required") or [])
        for part in node.get("allOf") or []:
            sub_properties, sub_required = self.merged(part)
            properties.update(sub_properties)
            required |= sub_required
        return properties, required

    def types_of(self, node):
        node = self.resolve(node)
        found = node.get("type")
        types = set(found if isinstance(found, list) else [found] if found else [])
        for key in ("oneOf", "anyOf", "allOf"):
            for part in node.get(key) or []:
                types |= self.types_of(part)
        if not types and node.get("properties"):
            types.add("object")
        return types - {None}

    def object_part(self, node):
        node = self.resolve(node)
        if node.get("properties") or node.get("allOf"):
            return node
        for key in ("oneOf", "anyOf"):
            for part in node.get(key) or []:
                part = self.resolve(part)
                if part.get("properties") or part.get("allOf"):
                    return part
        return node

    def check_members(self, where, schema, members):
        properties, required = self.merged(self.object_part(schema))
        for name, (kind, needed) in members.items():
            if name not in properties:
                self.problems.append(f"{where}: no member '{name}'")
                continue
            prop = properties[name]
            if needed and name not in required:
                self.problems.append(f"{where}: member '{name}' is not required")
            if kind is None:
                continue
            types = self.types_of(prop)
            if isinstance(kind, dict):
                if "object" not in types:
                    self.problems.append(f"{where}.{name}: not an object ({sorted(types)})")
                else:
                    self.check_members(f"{where}.{name}", prop, kind)
            elif isinstance(kind, list):
                if "array" not in types:
                    self.problems.append(f"{where}.{name}: not an array ({sorted(types)})")
                else:
                    self.check_members(f"{where}.{name}[]", self.resolve(prop).get("items", {}), kind[0])
            elif kind not in types and not (kind == N and I in types):
                self.problems.append(f"{where}.{name}: type {sorted(types)}, openblack reads {kind}")

    def check(self):
        paths = self.document.get("paths", {})
        for method, path, request, answer in CALLS:
            where = f"{method.upper()} {path}"
            operation = (paths.get(path) or {}).get(method)
            if operation is None:
                self.problems.append(f"{where}: not in the document")
                continue
            if request is not None:
                content = (operation.get("requestBody") or {}).get("content", {})
                schema = (content.get("application/json") or {}).get("schema")
                if schema is None:
                    self.problems.append(f"{where}: no JSON request body")
                else:
                    self.check_members(f"{where} request", schema, request)
            if answer is not None:
                schemas = []
                for code, response in (operation.get("responses") or {}).items():
                    if str(code).startswith("2"):
                        content = self.resolve(response).get("content", {})
                        if "application/json" in content:
                            schemas.append(content["application/json"].get("schema", {}))
                if not schemas:
                    self.problems.append(f"{where}: no JSON answer")
                for schema in schemas:
                    self.check_members(f"{where} answer", schema, answer)
        for method, path in NOT_IN_CONTRACT:
            state = "now in the document" if (paths.get(path) or {}).get(method) else "not in the document yet"
            print(f"note: {method.upper()} {path}: {state}")
        return self.problems


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    with open(sys.argv[1], encoding="utf-8") as file:
        document = yaml.safe_load(file)
    problems = Checker(document).check()
    for problem in problems:
        print(problem)
    print(f"check_openapi: {len(CALLS)} calls, {len(problems)} problems")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
