# gdsqlite
A very simple sqlite low level wrapper

```ts
let db = SQLiteDatabase.open("res://test.db", true);
let statement = db.prepare("SELECT * FROM test");
while (statement.step() == GError.OK) {
    let col = statement.column_count();
    let row = "";
    for (let i = 0; i < col; ++i) {
        row += var_to_str(statement.column_any(i)) + ",";
    }
    console.log("ROW: " + row);
}
// db.close();
```