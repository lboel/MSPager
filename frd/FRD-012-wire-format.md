# FRD-012 — Wire format

| | |
|---|---|
| Status | Partially implemented (M2: sender/mention/body. Position: M3) |
| Milestone | M2, M3 |
| PRD | §9 (Interoperability, Message size) |

## Requirement
1. Pager messages SHALL be standard MeshCore **`PAYLOAD_TYPE_GRP_TXT`** messages (`TXT_TYPE_PLAIN`) on the pager channel.
2. Decrypted text grammar (ABNF-ish):
   ```
   message   = sender ": " [mention] body [" " position]
   sender    = 1*31(any byte except ":")          ; node name, added by sendGroupMessage
   mention   = "@[" 1*31(any byte except "]") "] "
   body      = canned / freetext                   ; canned per FRD-006
   position  = "[" ( coord "," coord [" ~" age] / "no GPS" ) "]"
   coord     = ["-"] 1*3DIGIT "." 4DIGIT
   age       = 1*3DIGIT ("min" / "h" / "d")
   ```
3. Receive parse (reference regex, applied to the decrypted text):
   ```
   ^([^:]+): (?:@\[([^\]]+)\] )?(.*?)(?: \[(?:(-?\d{1,3}\.\d{1,6}),(-?\d{1,3}\.\d{1,6})(?: ~(\d+)(min|h|d))?|no GPS)\])?$
   ```
   The receiver SHALL accept 1–6 decimals (tolerant of other senders). Free text that doesn't match SHALL be shown verbatim with no position.
4. **Length budget** (must stay below `MAX_TEXT_LEN` = 160 bytes including the sender prefix):

   | Part | Worst case (bytes) |
   |---|---|
   | sender + `": "` | 31 + 2 |
   | mention `@[…] ` | 31 + 3 |
   | body (`Angekommen?`) | 11 |
   | position ` [-12.3456,-123.4567 ~999min]` | 29 |
   | **Total** | **107** (< 160 ✔) |

## Examples
```
Anna: Angekommen? [52.5201,13.4050]
Ben: Brauche Hilfe [52.5163,13.3777 ~12min]
Cleo: @[Anna] Ja [no GPS]
Anna: @[Ben] 📞 [52.5201,13.4050]
```
