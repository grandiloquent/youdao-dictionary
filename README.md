# 有道字典数据库

使用 C 语言通过有道词典查询整本书的单词，格式化查询结果后，写入 SQLite 数据库。

可导出数据 制作 Kindle 电纸书字典

## 编译 mbedtls

1. `C:\msys64\msys2_shell.cmd -mingw64`
2. `cd <path>/mbedtls/library`
3. `make LDFLAGS="-Wl,-rpath '-Wl,\$\$ORIGIN'" -f Makefile.mk SHARED=all WINDOWS_BUILD=dll`

## 编译

```sh
$ gcc -lws2_32 -lpthread -I../mbedtls/include main.c -o main.exe && main.exe
```

## 词典

下载 [youdao.db](https://github.com/grandiloquent/youdao-dictionary/blob/master/youdao.db)

```sql
$ select count(*) from dic
$ select key,count(key),max(length(key)) from dic order by length(key) desc
$ select key,length(key),count(key) from dic GROUP by length(key) ORDER by count(key) desc
```

包含 `48254` 个单词

## 第三方类库

- https://github.com/sqlite/sqlite
- https://github.com/ARMmbed/mbedtls
- https://github.com/DaveGamble/cJSON

