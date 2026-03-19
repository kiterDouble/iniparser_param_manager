/**
 * @file iniparser.c
 * @brief 简化版INI文件解析器实现
 * @description 基于原iniparser的简化实现，移除外部字典依赖
 */

#include "iniparser.h"
#include <ctype.h>
#include <errno.h>

#define ASCIILINESZ     INI_LINE_LEN

/* 将字符串转换为小写（线程安全版本） */
static const char *strlwc(const char *in, char *out, unsigned len)
{
    unsigned i;
    if (in == NULL || out == NULL || len == 0) return NULL;
    i = 0;
    while (in[i] != '\0' && i < len - 1) {
        out[i] = (char)tolower((int)in[i]);
        i++;
    }
    out[i] = '\0';
    return out;
}

/* 去除字符串首尾空白 */
static unsigned strstrip(char *s)
{
    char *last = NULL;
    char *dest = s;

    if (s == NULL) return 0;

    last = s + strlen(s);
    while (isspace((int)*s) && *s) s++;
    while (last > s) {
        if (!isspace((int)*(last - 1)))
            break;
        last--;
    }
    *last = '\0';

    memmove(dest, s, last - s + 1);
    return (unsigned)(last - s);
}

/* 创建新字典 */
static dictionary *dictionary_new(void)
{
    dictionary *d = (dictionary *)malloc(sizeof(dictionary));
    if (d) {
        memset(d, 0, sizeof(dictionary));
        d->count = 0;
    }
    return d;
}

/* 在字典中查找键 */
static int dictionary_find(dictionary *d, const char *key)
{
    int i;
    char lkey[INI_KEY_LEN];
    
    if (d == NULL || key == NULL) return -1;
    
    strlwc(key, lkey, sizeof(lkey));
    for (i = 0; i < d->count; i++) {
        if (d->entries[i].key[0] == '\0') continue;
        if (!strcmp(d->entries[i].key, lkey)) {
            return i;
        }
    }
    return -1;
}

/* 设置字典键值 */
static int dictionary_set(dictionary *d, const char *key, const char *value)
{
    int idx;
    char lkey[INI_KEY_LEN];
    
    if (d == NULL || key == NULL) return -1;
    
    strlwc(key, lkey, sizeof(lkey));
    
    /* 查找是否已存在 */
    idx = dictionary_find(d, key);
    if (idx >= 0) {
        /* 更新现有值 */
        if (value) {
            strncpy(d->entries[idx].value, value, INI_VALUE_LEN - 1);
            d->entries[idx].value[INI_VALUE_LEN - 1] = '\0';
        } else {
            d->entries[idx].key[0] = '\0';  /* 删除 */
        }
        return 0;
    }
    
    /* 添加新键值对 */
    if (d->count >= INI_MAX_ENTRIES) return -1;
    if (value == NULL) return 0;  /* 删除不存在的键 */
    
    strncpy(d->entries[d->count].key, lkey, INI_KEY_LEN - 1);
    d->entries[d->count].key[INI_KEY_LEN - 1] = '\0';
    strncpy(d->entries[d->count].value, value, INI_VALUE_LEN - 1);
    d->entries[d->count].value[INI_VALUE_LEN - 1] = '\0';
    d->count++;
    
    return 0;
}

/* 获取字典值 */
static char *dictionary_get(dictionary *d, const char *key, char *def)
{
    int idx;
    char lkey[INI_KEY_LEN];
    
    if (d == NULL || key == NULL) return def;
    
    strlwc(key, lkey, sizeof(lkey));
    
    idx = dictionary_find(d, key);
    if (idx >= 0) {
        return d->entries[idx].value;
    }
    return def;
}

/* 从行解析section */
static int iniparser_parse_section(const char *line, char *section, int len)
{
    int i = 0;
    int j = 0;
    int start = 0;
    
    if (line[0] != '[') return 0;
    
    /* 跳过开头的'[' */
    i = 1;
    while (line[i] && isspace((int)line[i])) i++;
    start = i;
    
    /* 找到']' */
    while (line[i] && line[i] != ']' && i < len - 1) i++;
    
    if (line[i] != ']') return 0;
    
    /* 复制section名 */
    j = 0;
    while (start < i && j < len - 1) {
        section[j++] = line[start++];
    }
    section[j] = '\0';
    strstrip(section);
    
    return (section[0] != '\0') ? 1 : 0;
}

/* 从行解析键值对 */
static int iniparser_parse_keyval(const char *line, char *key, int key_len,
                                   char *value, int val_len)
{
    int i = 0;
    int j;
    
    /* 跳过前导空格 */
    while (line[i] && isspace((int)line[i])) i++;
    
    /* 跳过空行和注释 */
    if (line[i] == '\0' || line[i] == '#' || line[i] == ';') return 0;
    
    /* 找到等号 */
    j = i;
    while (line[j] && line[j] != '=') j++;
    
    if (line[j] != '=') return 0;
    
    /* 提取key */
    strncpy(key, line + i, j - i);
    key[j - i] = '\0';
    strstrip(key);
    if (key[0] == '\0') return 0;
    
    /* 提取value */
    j++;
    while (line[j] && isspace((int)line[j])) j++;
    strncpy(value, line + j, val_len - 1);
    value[val_len - 1] = '\0';
    strstrip(value);
    
    return 1;
}

/* 加载INI文件 */
dictionary* iniparser_load(const char *ininame)
{
    FILE *in;
    char line[ASCIILINESZ];
    char section[ASCIILINESZ];
    char key[ASCIILINESZ];
    char val[ASCIILINESZ];
    char fullkey[ASCIILINESZ];
    dictionary *d;
    int lineno = 0;
    
    if ((in = fopen(ininame, "r")) == NULL) {
        return NULL;
    }
    
    d = dictionary_new();
    if (!d) {
        fclose(in);
        return NULL;
    }
    
    memset(section, 0, sizeof(section));
    
    while (fgets(line, ASCIILINESZ, in) != NULL) {
        lineno++;
        
        /* 去掉换行符 */
        line[strcspn(line, "\n\r")] = '\0';
        
        /* 解析section */
        if (iniparser_parse_section(line, section, sizeof(section))) {
            continue;
        }
        
        /* 解析键值对 */
        if (iniparser_parse_keyval(line, key, sizeof(key), val, sizeof(val))) {
            if (section[0] != '\0') {
                snprintf(fullkey, sizeof(fullkey), "%s:%s", section, key);
            } else {
                strncpy(fullkey, key, sizeof(fullkey) - 1);
                fullkey[sizeof(fullkey) - 1] = '\0';
            }
            dictionary_set(d, fullkey, val);
        }
    }
    
    fclose(in);
    return d;
}

/* 释放字典 */
void iniparser_freedict(dictionary *d)
{
    if (d) {
        free(d);
    }
}

/* 获取字符串值 */
char* iniparser_getstring(dictionary *d, const char *key, char *def)
{
    return dictionary_get(d, key, def);
}

/* 获取整型值 */
int iniparser_getint(dictionary *d, const char *key, int notfound)
{
    char *str;
    
    str = iniparser_getstring(d, key, NULL);
    if (str == NULL) return notfound;
    
    return (int)strtol(str, NULL, 0);
}

/* 获取双精度浮点值 */
double iniparser_getdouble(dictionary *d, const char *key, double notfound)
{
    char *str;
    
    str = iniparser_getstring(d, key, NULL);
    if (str == NULL) return notfound;
    
    return strtod(str, NULL);
}

/* 获取布尔值 */
int iniparser_getboolean(dictionary *d, const char *key, int notfound)
{
    char *str;
    char lc[ASCIILINESZ];
    
    str = iniparser_getstring(d, key, NULL);
    if (str == NULL) return notfound;
    
    strlwc(str, lc, sizeof(lc));
    
    if (strcmp(lc, "true") == 0 || strcmp(lc, "yes") == 0 || 
        strcmp(lc, "on") == 0 || strcmp(lc, "1") == 0) {
        return 1;
    }
    if (strcmp(lc, "false") == 0 || strcmp(lc, "no") == 0 || 
        strcmp(lc, "off") == 0 || strcmp(lc, "0") == 0) {
        return 0;
    }
    
    return notfound;
}

/* 设置字符串值 */
int iniparser_setstring(dictionary *d, const char *key, const char *value)
{
    return dictionary_set(d, key, value);
}

/* 保存字典到INI文件 */
void iniparser_dump_ini(dictionary *d, FILE *f)
{
    int i;
    char prev_section[ASCIILINESZ] = "";
    char *colon;
    int section_len;
    
    if (d == NULL || f == NULL) return;
    
    for (i = 0; i < d->count; i++) {
        if (d->entries[i].key[0] == '\0') continue;
        
        /* 找到section分隔符 */
        colon = strchr(d->entries[i].key, ':');
        if (colon) {
            section_len = (int)(colon - d->entries[i].key);
            
            /* 检查section是否变化 */
            if (strncmp(prev_section, d->entries[i].key, section_len) != 0 ||
                prev_section[section_len] != '\0') {
                strncpy(prev_section, d->entries[i].key, section_len);
                prev_section[section_len] = '\0';
                fprintf(f, "\n[%s]\n", prev_section);
            }
            fprintf(f, "%s = %s\n", colon + 1, d->entries[i].value);
        } else {
            /* 无section */
            fprintf(f, "%s = %s\n", d->entries[i].key, d->entries[i].value);
        }
    }
}

/* 获取section数量 */
int iniparser_getnsec(dictionary *d)
{
    int i;
    int nsec = 0;
    char sections[INI_MAX_ENTRIES][INI_KEY_LEN];
    int sec_count = 0;
    int j, found;
    char section[INI_KEY_LEN];
    char *colon;
    
    if (d == NULL) return -1;
    
    for (i = 0; i < d->count; i++) {
        if (d->entries[i].key[0] == '\0') continue;
        
        colon = strchr(d->entries[i].key, ':');
        if (!colon) continue;
        
        /* 提取section名 */
        int len = (int)(colon - d->entries[i].key);
        strncpy(section, d->entries[i].key, len);
        section[len] = '\0';
        
        /* 检查是否已统计 */
        found = 0;
        for (j = 0; j < sec_count; j++) {
            if (strcmp(sections[j], section) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found && sec_count < INI_MAX_ENTRIES) {
            strncpy(sections[sec_count], section, INI_KEY_LEN - 1);
            sections[sec_count][INI_KEY_LEN - 1] = '\0';
            sec_count++;
            nsec++;
        }
    }
    
    return nsec;
}

/* 获取section名称 */
char* iniparser_getsecname(dictionary *d, int n)
{
    static char section[INI_KEY_LEN];
    int i;
    int curr_sec = 0;
    char *colon;
    char found_sections[INI_MAX_ENTRIES][INI_KEY_LEN];
    int sec_count = 0;
    int j, found;
    
    if (d == NULL || n < 0) return NULL;
    
    memset(section, 0, sizeof(section));
    
    for (i = 0; i < d->count; i++) {
        if (d->entries[i].key[0] == '\0') continue;
        
        colon = strchr(d->entries[i].key, ':');
        if (!colon) continue;
        
        int len = (int)(colon - d->entries[i].key);
        strncpy(section, d->entries[i].key, len);
        section[len] = '\0';
        
        /* 检查是否已记录 */
        found = 0;
        for (j = 0; j < sec_count; j++) {
            if (strcmp(found_sections[j], section) == 0) {
                found = 1;
                break;
            }
        }
        
        if (!found && sec_count < INI_MAX_ENTRIES) {
            strncpy(found_sections[sec_count], section, INI_KEY_LEN - 1);
            found_sections[sec_count][INI_KEY_LEN - 1] = '\0';
            
            if (curr_sec == n) {
                return found_sections[sec_count];
            }
            sec_count++;
            curr_sec++;
        }
    }
    
    return NULL;
}

/* 查找entry */
int iniparser_find_entry(dictionary *d, const char *sec, const char *key)
{
    char fullkey[INI_KEY_LEN];
    
    if (d == NULL || key == NULL) return 0;
    
    if (sec && sec[0]) {
        snprintf(fullkey, sizeof(fullkey), "%s:%s", sec, key);
    } else {
        strncpy(fullkey, key, sizeof(fullkey) - 1);
        fullkey[sizeof(fullkey) - 1] = '\0';
    }
    
    return (dictionary_find(d, fullkey) >= 0) ? 1 : 0;
}
