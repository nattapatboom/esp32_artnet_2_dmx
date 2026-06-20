# Handover System

ระบบติดตามการทำงานของโปรเจกต์ — แต่ละ version เก็บเป็นไฟล์แยกในโฟลเดอร์นี้

## Version Format

ใช้รูปแบบ `x.xx.xx` (major.minor.patch):
- **major**: เปลี่ยนเมื่อ architecture หรือ project structure เปลี่ยน
- **minor**: เปลี่ยนเมื่อมี feature ใหม่ หรือ milestone ใหม่
- **patch**: เปลี่ยนเมื่อแก้ไข content, เพิ่ม/ลบ section ใน handover

## กฎ

- **ห้ามแก้ไขไฟล์เก่าเด็ดขาด** — เมื่อต้องการอัปเดต ให้สร้างไฟล์ version ใหม่
- แต่ละไฟล์ต้องมี **ไม่เกิน 1000 บรรทัด**
- แต่ละ version ต้อง reference version ก่อนหน้าด้วยหมายเลขและ path
- ถ้าถึง 1000 บรรทัดแล้วยังมี content ไม่พอ ให้ย้าย content เก่าไป `archive/{version}.md` แล้วเขียน version ใหม่ reference archive แทน

## Structure

```
handover/
  README.md              # นี้
  archive/               # เก็บ drafts, backups, และ version เก่าที่ full content เกิน 1000 บรรทัด
  1.xx.xx.md             # version file
  ...
```

## Index

| Version | Date | Description |
|---------|------|-------------|
| 1.00.00 | — | Initial handover — Art-Net Node Device (WT32-ETH01) firmware structure |
| 1.01.00 | — | — |
| 1.02.00 | — | — |
| 1.03.00 | — | — |
| 1.04.00 | — | — |
| 1.05.00 | — | — |
| 1.06.00 | — | — |
| 1.07.00 | — | — |
| 1.08.00 | — | — |
| 1.09.00 | — | — |
| 1.10.00 | — | — |
| 1.11.00 | — | Phase 1 Cleanup + PCA9685 Expansion Planning |
| 1.12.00 | — | Consolidated Status & Architecture Design |
| 1.13.00 | — | Pending Features & Next Phase Roadmap |
| 1.14.00 | — | Pin Mapping UI, I2C Mutex, Display, ZC Safety, Scoring |
| 1.15.00 | 2026-06-18 | DFPlayer MP3 (Type 15), Hardware Conflict Matrix, Code Review, Dead Code Cleanup Policy |
