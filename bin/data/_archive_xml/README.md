# Archived XML settings (pre-conversion backup)

Copied from this stick's live performer folders on 2026-11-06, before
running `tools/convert_xml_settings.py` on this stick (labelled EMI_GAUGE).
Nothing was moved or deleted from the live folders - this is a pure backup.

Recorded per performer so a later pass across other units' sticks can tell
which copy of a given performer is genuinely the newest edit, rather than
guessing. These are each performer's own latest XML file mtime, which the
FAT filesystem preserves honestly (unlike a git checkout, which resets
mtimes to checkout time) - go by these, not by anything in the git repo's
own working tree.

| Performer | Latest XML edit (this stick) | Notes |
|---|---|---|
| Chandra | 2018-11-06 23:11 | Untouched since initial setup. |
| Clement | 2021-08-19 01:00 | Most files 2018, some re-edited 2021. |
| Emi     | 2021-08-21 23:29 | **Every file except appSettings is named `Fred_*`, not `Emi_*`** - likely an old copy-from-Fred that was never renamed on this stick (the laptop's already-converted copy has this fixed - compare before trusting this stick's Emi data as "final"). |
| Fred    | 2023-02-14 04:12 | Newest of the five; grain presets from 2022-06, effects/app settings re-touched 2023-02. |
| Miguel  | 2018-11-06 23:12 | Untouched since initial setup. |
