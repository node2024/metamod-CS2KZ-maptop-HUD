"""Execute the generated production SQL against adversarial SQLite rankings."""
import sqlite3
import subprocess
import sys

db = sqlite3.connect(":memory:")
db.executescript("""
CREATE TABLE Maps(ID INTEGER,Name TEXT);
CREATE TABLE MapCourses(ID INTEGER,MapID INTEGER,Name TEXT,StageID INTEGER);
CREATE TABLE Modes(ID INTEGER,Name TEXT,ShortName TEXT);
CREATE TABLE Players(SteamID64 INTEGER,Alias TEXT);
CREATE TABLE Bans(ID INTEGER,SteamID64 INTEGER,ExpiresAt TEXT);
CREATE TABLE Times(ID TEXT,SteamID64 INTEGER,MapCourseID INTEGER,ModeID INTEGER,StyleIDFlags INTEGER,RunTime REAL,Teleports INTEGER);
INSERT INTO Maps VALUES(1,'kz_test');
INSERT INTO MapCourses VALUES(1,1,'word''s backyard',1),(2,1,'Bonus',2);
INSERT INTO Modes VALUES(1,'Classic','CKZ'),(2,'Vanilla','VNL');
""")
for i in range(1, 20):
    db.execute("INSERT INTO Players VALUES(?,?)", (i, f"Player {i}"))
    db.execute("INSERT INTO Times VALUES(?,?,?,?,?,?,?)", (f"run-{i:02}", i, 1, 1, 0, 100 + i, 0))
db.executescript("""
INSERT INTO Times VALUES('faster-tp',1,1,1,0,90,2);
INSERT INTO Times VALUES('duplicate-pro',2,1,1,0,102,0);
INSERT INTO Times VALUES('styled',2,1,1,1,10,0);
INSERT INTO Times VALUES('other-mode',3,1,2,0,11,0);
INSERT INTO Times VALUES('vnl-unbanned',8,1,2,0,12,0);
INSERT INTO Times VALUES('other-course',4,2,1,0,12,0);
INSERT INTO Bans VALUES(1,3,NULL),(2,4,'2099-01-01 00:00:00'),(3,5,'2000-01-01 00:00:00');
ALTER TABLE Times ADD COLUMN Metadata TEXT;
ALTER TABLE Times ADD COLUMN Created TEXT;
UPDATE Times SET Metadata='{"splitZoneTimes":[30,60]}',Created='2026-10-24 12:34:56' WHERE ID='faster-tp';
""")
def query(category, offset=0, mode="classic", page_size=5):
    sql = subprocess.check_output([sys.argv[1], "--sql", category, str(offset), mode, str(page_size)], text=True)
    return db.execute(sql).fetchall()

overall = query("overall")
pro = query("pro")
assert overall[0][0] == "faster-tp", overall
assert overall[0][5:] == ('{"splitZoneTimes":[30,60]}', '2026-10-24 12:34:56')
assert pro[0][0] == "run-01", pro
assert len([row for row in pro if row[1] == 2]) == 1, pro
assert all(row[1] not in (3, 4) for row in overall + pro)
assert any(row[1] == 5 for row in overall)
assert len(overall) == 6  # one lookahead row beyond the five shown
assert query("overall", 5)[0] == overall[5]
assert all(row[0] not in ("styled", "other-mode", "other-course", "vnl-unbanned") for row in overall + pro)
assert [row[0] for row in query("overall", mode="vanilla")] == ["vnl-unbanned"]
for category in ("overall", "pro"):
    for mode, expected in (("classic", 17), ("vanilla", 1)):
        sql = subprocess.check_output([sys.argv[1], "--count-sql", category, "0", mode], text=True)
        assert db.execute(sql).fetchone()[0] == expected
for category in ("overall", "pro"):
    first = query(category, page_size=15)
    last = query(category, offset=15, page_size=15)
    assert len(first) == 16 and len(last) == 2
    assert first[15] == last[0]
    all_rows = first[:15] + last
    assert len(all_rows) == len({row[1] for row in all_rows}) == 17
print("local ranking / PRO / ties / bans / styles / courses / Studio and Table pagination / mode selection: OK")
