# Test Run Records

Store completed checklist copies in this directory.

File naming convention:

```text
YYYY-MM-DD_HHMM_test_checklist.md
```

Example:

```text
2026-06-24_1530_test_checklist.md
```

Workflow:

- Create a run file before starting a verification pass:

```powershell
& "C:\Users\natta\.platformio\penv\Scripts\python.exe" tools/create_test_run.py --tester "<name>" --device-ip "<ip or note>" --hardware-setup "<setup>" --scope "<scope>"
```

- If the helper is unavailable, copy `docs/test_checklist.md` into this directory manually.
- Fill in date/time, firmware commit, firmware version/build, tester, device/IP, hardware setup, and test scope.
- Mark every item as completed, failed, or not tested in the copied file.
- Record incomplete or failed items so the next run can confirm whether previous verification was complete.
- Add newly discovered missing test coverage under `Additional Test Items Found`, then update `docs/test_checklist.md` if the item should become part of the permanent checklist.
