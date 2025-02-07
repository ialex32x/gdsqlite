#ifndef GDSQLITE_DATABASE_H
#define GDSQLITE_DATABASE_H

#include "../sqlite/sqlite3.h"
#include "core/object/ref_counted.h"

class SQLiteStatement;

class SQLiteDatabase : public RefCounted
{
    GDCLASS(SQLiteDatabase, RefCounted)
    friend class SQLiteStatement;

private:
    sqlite3* db_ = nullptr;

protected:
    static void _bind_methods();

public:
    static Ref<SQLiteDatabase> open(const String& p_path, bool p_readonly);
    void close();

    Ref<SQLiteStatement> prepare(const String& p_query);

    Error exec(const String& p_query);

    virtual ~SQLiteDatabase() override;
};

#endif
