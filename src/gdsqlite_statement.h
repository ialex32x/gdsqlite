#ifndef GDSQLITE_STATEMENT_H
#define GDSQLITE_STATEMENT_H

#include "../sqlite/sqlite3.h"
#include "core/object/ref_counted.h"

class SQLiteDatabase;

class SQLiteStatement : public RefCounted
{
    GDCLASS(SQLiteStatement, RefCounted)
    friend class SQLiteDatabase;

private:
    Ref<SQLiteDatabase> db_;
    sqlite3_stmt* stmt_ = nullptr;

    void init(SQLiteDatabase* p_db);
    Error prepare(const String& p_query);

    void _close();
    void _on_db_closed();

protected:
    static void _bind_methods();

public:
    Error step();

    Error reset();

    Error bind_any(int p_index, Variant p_value);
    Error bind_null(int p_index);
    Error bind_bool(int p_index, bool p_value);
    Error bind_int(int p_index, int64_t p_value);
    Error bind_double(int p_index, double p_value);
    Error bind_text(int p_index, const String& p_value);
    Error bind_blob(int p_index, const PackedByteArray& p_value);

    int column_count() const;
    Variant::Type column_type(int p_index) const;

    Variant column_any(int p_index) const;
    bool column_bool(int p_index) const;
    int64_t column_int(int p_index) const;
    double column_double(int p_index) const;
    String column_text(int p_index) const;
    PackedByteArray column_blob(int p_index) const;

    virtual ~SQLiteStatement() override;
};

#endif
