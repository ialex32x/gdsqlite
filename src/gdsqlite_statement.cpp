#include "gdsqlite_statement.h"
#include "gdsqlite_database.h"

void SQLiteStatement::_bind_methods()
{
    ClassDB::bind_method(D_METHOD("step"), &SQLiteStatement::step);
    ClassDB::bind_method(D_METHOD("reset"), &SQLiteStatement::reset);

    ClassDB::bind_method(D_METHOD("bind_any", "index", "value"), &SQLiteStatement::bind_any);
    ClassDB::bind_method(D_METHOD("bind_null", "index"), &SQLiteStatement::bind_null);
    ClassDB::bind_method(D_METHOD("bind_bool", "index", "value"), &SQLiteStatement::bind_bool);
    ClassDB::bind_method(D_METHOD("bind_int", "index", "value"), &SQLiteStatement::bind_int);
    ClassDB::bind_method(D_METHOD("bind_double", "index", "value"), &SQLiteStatement::bind_double);
    ClassDB::bind_method(D_METHOD("bind_text", "index", "value"), &SQLiteStatement::bind_text);
    ClassDB::bind_method(D_METHOD("bind_blob", "index", "value"), &SQLiteStatement::bind_blob);

    ClassDB::bind_method(D_METHOD("column_count"), &SQLiteStatement::column_count);
    ClassDB::bind_method(D_METHOD("column_type", "index"), &SQLiteStatement::column_type);

    ClassDB::bind_method(D_METHOD("column_any", "index"), &SQLiteStatement::column_any);
    ClassDB::bind_method(D_METHOD("column_bool", "index"), &SQLiteStatement::column_bool);
    ClassDB::bind_method(D_METHOD("column_int", "index"), &SQLiteStatement::column_int);
    ClassDB::bind_method(D_METHOD("column_double", "index"), &SQLiteStatement::column_double);
    ClassDB::bind_method(D_METHOD("column_text", "index"), &SQLiteStatement::column_text);
    ClassDB::bind_method(D_METHOD("column_blob", "index"), &SQLiteStatement::column_blob);
}

SQLiteStatement::~SQLiteStatement()
{
    const int rc = sqlite3_finalize(stmt_);
    ERR_FAIL_COND_MSG(rc != SQLITE_OK, String::utf8(sqlite3_errmsg(db_->db_)));
}

Error SQLiteStatement::step()
{
    const int rc = sqlite3_step(stmt_);
    return rc == SQLITE_ROW ? OK : rc == SQLITE_DONE ? ERR_UNAVAILABLE : FAILED;
}

Error SQLiteStatement::reset()
{
    const int rc = sqlite3_reset(stmt_);
    return rc == SQLITE_OK ? OK : FAILED;
}

Error SQLiteStatement::bind_null(int p_index)
{
    const int rc = sqlite3_bind_null(stmt_, p_index);
    return rc == SQLITE_OK ? OK : FAILED;
}

Error SQLiteStatement::bind_double(int p_index, double p_value)
{
    const int rc = sqlite3_bind_double(stmt_, p_index, p_value);
    return rc == SQLITE_OK ? OK : FAILED;
}

Error SQLiteStatement::bind_bool(int p_index, bool p_value)
{
    const int rc = sqlite3_bind_int64(stmt_, p_index, p_value);
    return rc == SQLITE_OK ? OK : FAILED;
}

Error SQLiteStatement::bind_int(int p_index, int64_t p_value)
{
    const int rc = sqlite3_bind_int64(stmt_, p_index, p_value);
    return rc == SQLITE_OK ? OK : FAILED;
}

Error SQLiteStatement::bind_text(int p_index, const String& p_value)
{
    const CharString u8 = p_value.utf8();
    const int rc = sqlite3_bind_text(stmt_, p_index, u8.get_data(), u8.length(), SQLITE_TRANSIENT);
    return rc == SQLITE_OK ? OK : FAILED;
}

Error SQLiteStatement::bind_blob(int p_index, const PackedByteArray& p_value)
{
    if (p_value.is_empty()) return bind_null(p_index);
    const int rc = sqlite3_bind_blob64(stmt_, p_index, p_value.ptr(), p_value.size(), SQLITE_TRANSIENT);
    return rc == SQLITE_OK ? OK : FAILED;
}

Error SQLiteStatement::prepare(const String& p_query)
{
    const CharString sql = p_query.utf8();
    const char *tail = nullptr;
    const int rc = sqlite3_prepare_v3(db_->db_, sql.get_data(), sql.length(), 0, &stmt_, &tail);
    ERR_FAIL_COND_V_MSG(rc != SQLITE_OK, FAILED, String::utf8(sqlite3_errmsg(db_->db_)));
    // pending_sql_ = String::utf8(tail);
    return OK;
}

int SQLiteStatement::column_count() const
{
    return sqlite3_column_count(stmt_);
}

Variant::Type SQLiteStatement::column_type(int p_index) const
{
    const int tp = sqlite3_column_type(stmt_, p_index);
    switch (tp)
    {
        case SQLITE_INTEGER: return Variant::INT;
        case SQLITE_FLOAT: return Variant::FLOAT;
        case SQLITE_TEXT: return Variant::STRING;
        case SQLITE_BLOB: return Variant::PACKED_BYTE_ARRAY;
        case SQLITE_NULL: return Variant::NIL;
        default: ERR_FAIL_V_MSG(Variant::NIL, "Unknown column type");
    }
}

bool SQLiteStatement::column_bool(int p_index) const
{
    return sqlite3_column_int64(stmt_, p_index) != 0;
}

int64_t SQLiteStatement::column_int(int p_index) const
{
    return sqlite3_column_int64(stmt_, p_index);
}

double SQLiteStatement::column_double(int p_index) const
{
    return sqlite3_column_double(stmt_, p_index);
}

String SQLiteStatement::column_text(int p_index) const
{
    const unsigned char* u8 = sqlite3_column_text(stmt_, p_index);
    return u8 ? String::utf8((const char*) u8) : String();
}

PackedByteArray SQLiteStatement::column_blob(int p_index) const
{
    const int n = sqlite3_column_bytes(stmt_, p_index);
    PackedByteArray buf;
    buf.resize(n);
    const void* blob = sqlite3_column_blob(stmt_, p_index);
    memcpy((void*) buf.ptr(), blob, n);
    return buf;
}

Error SQLiteStatement::bind_any(int p_index, Variant p_value)
{
    switch (p_value.get_type())
    {
        case Variant::NIL: return bind_null(p_index);
        case Variant::BOOL: return bind_bool(p_index, p_value);
        case Variant::INT: return bind_int(p_index, p_value);
        case Variant::FLOAT: return bind_double(p_index, p_value);
        case Variant::STRING:
        case Variant::STRING_NAME: return bind_text(p_index, p_value);
        case Variant::PACKED_BYTE_ARRAY: return bind_blob(p_index, p_value);
        default: ERR_FAIL_V_MSG(FAILED, "Unsupported value type");
    }
}

Variant SQLiteStatement::column_any(int p_index) const
{
    switch (column_type(p_index))
    {
        case Variant::NIL: return Variant();
        case Variant::BOOL: return column_bool(p_index);
        case Variant::INT: return column_int(p_index);
        case Variant::FLOAT: return column_double(p_index);
        case Variant::STRING: return column_text(p_index);
        case Variant::PACKED_BYTE_ARRAY: return column_blob(p_index);
        default: ERR_FAIL_V_MSG(Variant(), "Unknown column type");
    }
}

