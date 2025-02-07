#include "register_types.h"
#include "src/gdsqlite_database.h"
#include "src/gdsqlite_statement.h"

void initialize_gdsqlite_module(ModuleInitializationLevel p_level)
{
    if (p_level == MODULE_INITIALIZATION_LEVEL_SCENE)
    {
        GDREGISTER_CLASS(SQLiteDatabase)
        GDREGISTER_CLASS(SQLiteStatement)
	}
}

void uninitialize_gdsqlite_module(ModuleInitializationLevel p_level)
{

}

