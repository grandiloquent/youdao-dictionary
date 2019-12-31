# 有道字典数据库

使用 C 语言通过有道词典查询整本书的单词，格式化查询结果后，写入 SQLite 数据库。

## 编译 mbedtls

1. `C:\msys64\msys2_shell.cmd -mingw64`
2. `cd <path>/mbedtls/library`
3. `make LDFLAGS="-Wl,-rpath '-Wl,\$\$ORIGIN'" -f Makefile.mk SHARED=all WINDOWS_BUILD=dll`

## 编译

```sh
$ gcc -lws2_32 -lpthread -I../mbedtls/include main.c -o main.exe && main.exe
```

## 第三方类库

- https://github.com/sqlite/sqlite
- https://github.com/ARMmbed/mbedtls
- https://github.com/DaveGamble/cJSON

