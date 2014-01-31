#pragma once

#include "tes_binding.h"

#define MESSAGE(...) // _DMESSAGE(__VA_ARGS__);

namespace collections {

    const char *kCommentObject = "creates new container object. returns container identifier (integral number).\n"
                                 "identifier is the thing you will have to pass to the most of container's functions as first argument";

    class tes_object : public tes_binding::class_meta_mixin< tes_object > {
    public:

        REGISTER_TES_NAME("JValue");

        static const char * TesName() { return "JValue";}

        static HandleT retain(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            auto obj = collection_registry::getObject(handle);
            if (obj) {
                obj->tes_retain();
                return handle;
            }

            return HandleNull;
        }

        static HandleT autorelease(StaticFunctionTag*, HandleT handle) {
            MESSAGE(__FUNCTION__);
            autorelease_queue::instance().push(handle);
            return handle;
        }

        template<class T>
        static T* create() {
            MESSAGE(__FUNCTION__);
            return T::create();
        }

        template<class T>
        static object_base* object() {
            MESSAGE(__FUNCTION__);
            return T::object();
        }

        static void release(object_base *obj) {
            MESSAGE(__FUNCTION__);
            if (obj) {
                obj->tes_release();
            }
        }
        REGISTERF2(release, "*", "releases the object");

        static bool isArray(StaticFunctionTag*, HandleT handle) {
            object_base *obj = collection_registry::getObject(handle);
            return obj && obj->_type == CollectionTypeArray;
        }

        static bool isMap(StaticFunctionTag*, HandleT handle) {
            object_base *obj = collection_registry::getObject(handle);
            return obj && obj->_type == CollectionTypeMap;
        }

        static bool isFormMap(StaticFunctionTag*, HandleT handle) {
            object_base *obj = collection_registry::getObject(handle);
            return obj && obj->as<form_map>();
        }

        static HandleT readFromFile(StaticFunctionTag*, BSFixedString path) {
            if (path.data == nullptr)
                return 0;

            auto obj = json_parsing::readJSONFile(path.data);
            return  obj ? obj->id : 0;
        }

        static void writeToFile(StaticFunctionTag*, HandleT handle, BSFixedString path) {
            if (path.data == nullptr)  return;

            auto obj = collection_registry::getObject(handle);
            if (!obj)  return;


           
            std::unique_ptr<char, decltype(&free)> data(json_parsing::createJSONData(*obj), &free);
            //char * data = json_parsing::createJSONData(*obj);
            if (!data) return;

            //unique_ptr<FILE, decltype(&fclose)> 
            auto file = make_unique_file(fopen(path.data, "w"));
            if (!file) {
                //free(data);
                return;
            }

            fwrite(data.get(), 1, strlen(data.get()), file.get());
        }

        template<class T>
        static typename Tes2Value<T>::tes_type resolveT(StaticFunctionTag*, HandleT handle, BSFixedString path) {
            auto obj = collection_registry::getObject(handle);
            if (!obj) return 0;

            T val((T)0);
            json_parsing::resolvePath(obj, path.data, [&](Item* itmPtr) {
                if (itmPtr) {
                    val = itmPtr->readAs<T>();
                }
            });

            return typename Tes2Value<T>::tes_type(val);
        }

        static bool hasPath(StaticFunctionTag*, HandleT handle, BSFixedString path) {
            auto obj = collection_registry::getObject(handle);
            if (!obj) return false;

            bool succeed = false;
            json_parsing::resolvePath(obj, path.data, [&](Item* itmPtr) {
                succeed = (itmPtr != nullptr);
            });

            return succeed;
        }

        template<class T>
        static bool solveT(StaticFunctionTag*, HandleT handle, BSFixedString path, typename Tes2Value<T>::tes_type value) {
            auto obj = collection_registry::getObject(handle);
            if (!obj) return false;

            bool succeed = false;
            json_parsing::resolvePath(obj, path.data, [&](Item* itmPtr) {
                if (itmPtr) {
                    itmPtr->writeAs(value);
                    succeed = true;
                }
            });

            return succeed;
        }

        static void printMethod(const char *cname, const char *cargs) {
            //string args(cargs);

#define C2TES(c, tes) #c, #tes
            const char * type2tes[][2] = {
                C2TES(HandleT, int),
                C2TES(Index, int),
                C2TES(BSFixedString, string),
                C2TES(Float32, float),
                C2TES(SInt32, int),
                C2TES(TESForm*, form),
            };
#undef C2TES
            std::map<string,string> type2tesMap ;
            for (int i = 0; i < sizeof(type2tes) / (2 * sizeof(char*));++i) {
                type2tesMap[type2tes[i][0]] = type2tes[i][1];
            }


            string name;

            vector<string> strings; // #2: Search for tokens
            boost::split( strings, string(cargs), boost::is_any_of(", ") );

            auto strToStr = [&](const std::string& str) {
                return type2tesMap.find(str) != type2tesMap.end() ? type2tesMap[str] : str;
            };

            if (strings[0] != "void") {
                name += strToStr(strings[0]) + ' ';
            }

            name += "Function ";
            name += cname;
            name += "(";

            int argNum = 0;

            for (int i = 1; i < strings.size(); ++i, ++argNum) {
                if (strings[i].empty()) {
                    continue;
                }


                name += strToStr(strings[i]);
                name += " arg";
                name += (char)((argNum - 1) + '0');
                if ((i+1) < strings.size())
                    name += ", ";
            }

            name += ") global native\n";

            printf(name.c_str());
        }

#define ARGS(...) __VA_ARGS__

        static bool registerFuncs(VMClassRegistry* registry) {

            #define REGISTER2(name, func, argCount,  ... /*types*/ ) \
                if (registry) { \
                registry->RegisterFunction( \
                new NativeFunction ## argCount <StaticFunctionTag, __VA_ARGS__ >(name, TesName(), func, registry)); \
                registry->SetFunctionFlags(TesName(), name, VMClassRegistry::kFunctionFlag_NoWait); \
                }
                //printMethod(name, #__VA_ARGS__);

            #define REGISTER(func, argCount,  ... /*types*/ ) REGISTER2(#func, func, argCount, __VA_ARGS__)

 /*           auto addr = &proxy<decltype(&release), release>::tes_func_noreturn;

            registry->RegisterFunction(
                new NativeFunction1 <StaticFunctionTag, __VA_ARGS__ >("release", TesName(), addr, registry));
               // registry->SetFunctionFlags(TesName(), name, VMClassRegistry::kFunctionFlag_NoWait);*/

            //REGISTER(release, 1, void, HandleT);
            REGISTER(retain, 1, HandleT, HandleT);
            REGISTER(autorelease, 1, HandleT, HandleT);

            REGISTER(readFromFile, 1, HandleT, BSFixedString);
            REGISTER(writeToFile, 2, void, HandleT, BSFixedString);

            REGISTER(isArray, 1, bool, HandleT);
            REGISTER(isMap, 1, bool, HandleT);
            REGISTER(isFormMap, 1, bool, HandleT);

            REGISTER2("resolveVal", resolveT<Handle>, 2, HandleT, HandleT, BSFixedString);
            REGISTER2("resolveFlt", resolveT<Float32>, 2, Float32, HandleT, BSFixedString);
            REGISTER2("resolveStr", resolveT<BSFixedString>, 2, BSFixedString, HandleT, BSFixedString);
            REGISTER2("resolveInt", resolveT<SInt32>, 2, SInt32, HandleT, BSFixedString);

            bind(registry);

            return true;
        }
    };

    class tes_array : public tes_binding::class_meta_mixin< tes_array > {
    public:

        REGISTER_TES_NAME("JArray");

        static void additionalSetup() {
            metaInfo().extendsClass = "JValue";
        }

        static const char * TesName() { return "JArray";}

        typedef array::Index Index;

        static array* find(HandleT handle) {
            return collection_registry::getObjectOfType<array>(handle);
        }

        REGISTERF(tes_object::object<array>, "object", "", kCommentObject);

        template<class T>
        static object_base* fromArray(VMArray<T> arr) {
            auto obj = array::objectWithInitializer([&](array *me) {
                for (UInt32 i = 0; i < arr.Length(); ++i) {
                    T val;
                    arr.Get(&val, i);
                    me->_array.push_back(Item(val));
                }
            });

            return obj;
        }
        REGISTERF(fromArray<SInt32>, "objectWithInts", "values", "creates new array that contains given values");
        REGISTERF(fromArray<BSFixedString>, "objectWithStrings",  "values", NULL);
        REGISTERF(fromArray<Float32>, "objectWithFloats",  "values", NULL);
        REGISTERF(fromArray<bool>, "objectWithBooleans",  "values", NULL);

        template<class T>
        static T itemAtIndex(array *obj, Index index) {
            if (!obj) {
                return T(0);
            }

            mutex_lock g(obj->_mutex);
            return (index >= 0 && index < obj->_array.size()) ? obj->_array[index].readAs<T>() : T(0);
        }
        REGISTERF(itemAtIndex<SInt32>, "getInt", "* index", "returns value at index");
        REGISTERF(itemAtIndex<Float32>, "getFlt", "* index", "");
        REGISTERF(itemAtIndex<const char *>, "getStr", "* index", "");
        REGISTERF(itemAtIndex<Handle>, "getObj", "* index", "");
        REGISTERF(itemAtIndex<TESForm*>, "getForm", "* index", "");

        template<class T>
        static void replaceItemAtIndex(array *obj, Index index, T item) {
            if (!obj) {
                return;
            }

            mutex_lock g(obj->_mutex);
            if (index >= 0 && index < obj->_array.size()) {
                obj->_array[index] = Item(item);
            }
        }
        REGISTERF(replaceItemAtIndex<SInt32>, "setInt", "* index value", "replaces existing value at index with new value");
        REGISTERF(replaceItemAtIndex<Float32>, "setFlt", "* index value", "");
        REGISTERF(replaceItemAtIndex<const char *>, "setStr", "* index value", "");
        REGISTERF(replaceItemAtIndex<object_base*>, "setObj", "* index value", "");
        REGISTERF(replaceItemAtIndex<TESForm*>, "setForm", "* index value", "");

        template<class T>
        static void add(array *obj, T item) {
            MESSAGE(__FUNCTION__);
            if (obj) {
                mutex_lock g(obj->_mutex);
                obj->_array.push_back(Item(item));
            }
        }
        REGISTERF(add<SInt32>, "addInt", "* value", "appends value to the end of array");
        REGISTERF(add<Float32>, "addFlt", "* value", "");
        REGISTERF(add<const char *>, "addStr", "* value", "");
        REGISTERF(add<object_base*>, "addObj", "* value", "");
        REGISTERF(add<TESForm*>, "addForm", "* value", "");

        static Index count(array *obj) {
            MESSAGE(__FUNCTION__);
            if (obj) {
                mutex_lock g(obj->_mutex);
                return  obj->_array.size();
            }
            return 0;
        }
        REGISTERF2(count, "*", "inserted items count");

        static void clear(array *obj) {
            if (obj) {
                mutex_lock g(obj->_mutex);
                obj->_array.clear();
            }
        }
        REGISTERF2(clear, "*", "remove all items from array");

        static void eraseIndex(array *obj, SInt32 index) {
            if (obj) {
                mutex_lock g(obj->_mutex);
                if (index >= 0 && index < obj->_array.size()) {
                    obj->_array.erase(obj->_array.begin() + index);
                }
            }
        }
        REGISTERF2(eraseIndex, "* index", "erases item at index");

        static bool registerFuncs(VMClassRegistry* registry) {
            MESSAGE("register array funcs");

            OutputDebugStringW(L"reg arr fun");

            bind(registry);

            return true;
        }
    };

    inline const char* tes_hash(const char* in) {
        return in;
    }

    inline UInt32 tes_hash(TESForm * in) {
        return in ? in->formID : 0; 
    }

    template<class Key, class Cnt>
    class tes_map_t : public tes_binding::class_meta_mixin< tes_map_t<Key, Cnt> > {
    public:

        static Cnt* find(HandleT handle) {
            return collection_registry::getObjectOfType<Cnt>(handle);
        }

        REGISTER_TES_NAME("tt");

        //REGISTERF(create<Cnt>, "create", "", "");
        REGISTERF(tes_object::object<Cnt>, "object", "", kCommentObject);

        template<class T>
        static T getItem(Cnt *obj, Key key) {
            if (!obj || !key) {
                return T(0);
            }

            mutex_lock g(obj->_mutex);
            auto itr = obj->cnt.find(tes_hash(key));
            return itr != obj->cnt.end() ? itr->second.readAs<T>() : T(0);
        }
        REGISTERF(getItem<SInt32>, "getInt", "object key", "returns value associated with key");
        REGISTERF(getItem<Float32>, "getFlt", "object key", "");
        REGISTERF(getItem<const char *>, "getStr", "object key", "");
        REGISTERF(getItem<Handle>, "getObj", "object key", "");
        REGISTERF(getItem<TESForm*>, "getForm", "object key", "");

        template<class T>
        static void setItem(Cnt *obj, Key key, T item) {
            if (!obj || !key) {
                return;
            }

            mutex_lock g(obj->_mutex);
            obj->cnt[tes_hash(key)] = Item((T)item);
        }
        REGISTERF(setItem<SInt32>, "setInt", "* key", "creates key-value association. replaces existing value if any");
        REGISTERF(setItem<Float32>, "setFlt", "* key", "");
        REGISTERF(setItem<const char *>, "setStr", "* key", "");
        REGISTERF(setItem<object_base*>, "setObj", "* key object2", "");
        REGISTERF(setItem<TESForm*>, "setForm", "* key", "");

        static bool hasKey(Cnt *obj, Key key) {
            if (!obj || !key) {
                return 0;
            }

            mutex_lock g(obj->_mutex);
            auto itr = obj->cnt.find(tes_hash(key));
            return itr != obj->cnt.end();
        }
        REGISTERF2(hasKey, "* key", "true, if something associated with key");

        static bool removeKey(Cnt *obj, Key key) {
            if (!obj || !key) {
                return 0;
            }

            mutex_lock g(obj->_mutex);
            auto itr = obj->cnt.find(tes_hash(key));
            bool hasKey = (itr != obj->cnt.end());

            obj->cnt.erase(itr);
            return hasKey;
        }
        REGISTERF2(removeKey, "* key", "destroys key-value association");

        static SInt32 count(Cnt *obj) {
            if (!obj) {
                return 0;
            }

            mutex_lock g(obj->_mutex);
            return obj->cnt.size();
        }
        REGISTERF2(count, "*", "count of items/associations");

        static void clear(Cnt *obj) {
            if (!obj) {
                return;
            }

            mutex_lock g(obj->_mutex);
            obj->cnt.clear();
        }
        REGISTERF2(clear, "*", "remove all items from map container");

        static bool registerFuncs(VMClassRegistry* registry) {
            bind(registry);
            return true;
        }

        static void additionalSetup();
    };

    typedef tes_map_t<const char*, map > tes_map;
    typedef tes_map_t<TESForm *, form_map> tes_form_map;

    void tes_map::additionalSetup() {
        metaInfo().className = "JMap";
        metaInfo().extendsClass = "JValue";
    }

    void tes_form_map::additionalSetup() {
        metaInfo().className = "JFormMap";
        metaInfo().extendsClass = "JValue";
    }

    class tes_db : public tes_binding::class_meta_mixin<tes_db> {
    public:

        REGISTER_TES_NAME("JDB");

        static void additionalSetup() {}

        static const char * TesName() { return "JDB";}
        
        template<class T>
        static typename Tes2Value<T>::tes_type solveGetter(StaticFunctionTag* tag, BSFixedString path) {
            return tes_object::resolveT<T>(tag, shared_state::instance().databaseId(), path); 
        }

        template<class T>
        static bool solveSetter(StaticFunctionTag*, BSFixedString path, typename Tes2Value<T>::tes_type value) { 
            auto obj = collection_registry::getObject(shared_state::instance().databaseId());
            if (!obj) return false;

            bool succeed = false;
            json_parsing::resolvePath(obj, path.data, [&](Item* itmPtr) {
                if (itmPtr) {
                    *itmPtr = Item((T)value);
                    succeed = true;
                }
            });

            return succeed;
        }

        static void setValue(const char *path, object_base *obj) {
            object_base *db = shared_state::instance().database();
            map *dbMap = db ? db->as<map>() : nullptr;

            if (!dbMap) {
                return;
            }

            if (obj) {
                tes_map::setItem(dbMap, path, obj);
            } else {
                tes_map::removeKey(dbMap, path);
            }
        }
        REGISTERF(setValue, "setObj", "key object", "");

        static bool hasPath(StaticFunctionTag*tag, BSFixedString path) {
            return tes_object::hasPath(tag, shared_state::instance().databaseId(), path);
        }

        static void writeToFile(StaticFunctionTag* tag, BSFixedString path) {
            tes_object::writeToFile(tag, shared_state::instance().databaseId(), path);
        }

        static void readFromFile(/*StaticFunctionTag* tag,*/ BSFixedString path) {
            auto objNew = json_parsing::readJSONFile(path.data);
            shared_state::instance().setDataBase(objNew);
        }
        REGISTERF(readFromFile, "readFromFile", "path", "fills storage with JSON data");

        static bool registerFuncs(VMClassRegistry* registry) {

            REGISTER2("solveFltSetter", solveSetter<Float32>, 2, bool, BSFixedString, Float32);
            REGISTER2("solveIntSetter", solveSetter<SInt32>, 2, bool, BSFixedString, SInt32);
            REGISTER2("solveStrSetter", solveSetter<BSFixedString>, 2, bool, BSFixedString, BSFixedString);
            REGISTER2("solveObjSetter", solveSetter<Handle>, 2, bool, BSFixedString, HandleT);
            REGISTER2("solveFormSetter", solveSetter<TESForm*>, 2, bool, BSFixedString, TESForm*);

            REGISTER2("solveFlt", solveGetter<Float32>, 1, Float32, BSFixedString);
            REGISTER2("solveInt", solveGetter<SInt32>, 1, SInt32, BSFixedString);
            REGISTER2("solveStr", solveGetter<BSFixedString>, 1, BSFixedString, BSFixedString);
            REGISTER2("solveObj", solveGetter<Handle>, 1, HandleT, BSFixedString);
            REGISTER2("solveForm", solveGetter<TESForm*>, 1, TESForm*, BSFixedString);

            REGISTER(hasPath, 1, bool, BSFixedString);

            REGISTER(writeToFile, 1, void, BSFixedString);

            bind(registry);

            return true;
        }
    };


    bool registerFuncs(VMClassRegistry *registry) {
        collections::tes_array::registerFuncs(registry);

        collections::tes_map::registerFuncs(registry);
        collections::tes_form_map::registerFuncs(registry);

        collections::tes_object::registerFuncs(registry);

        collections::tes_db::registerFuncs(registry);

        return true;
    }

    void registerFuncsHook(VMClassRegistry **registryPtr) {
        registerFuncs(*registryPtr);
    }
}
