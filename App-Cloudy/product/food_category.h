// App-Cloudy/product/food_category.h
// 食材类别定义 — 7种类别、默认保质期、自动延期天数
// 参考：Q1_功能需求文档.md FR-02

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 食材类别数量（不含确认键）
#define FOOD_CATEGORY_COUNT     7

// 食材类别编号
typedef enum {
    FOOD_CAT_DAIRY = 1,    // 乳制品
    FOOD_CAT_MEAT = 2,     // 肉蛋类
    FOOD_CAT_VEG = 3,      // 蔬菜类
    FOOD_CAT_FRUIT = 4,    // 水果类
    FOOD_CAT_SEAFOOD = 5,  // 海鲜类
    FOOD_CAT_DRINK = 6,    // 饮品类
    FOOD_CAT_FROZEN = 7,   // 冷冻类
} food_category_t;

// 食材类别配置
typedef struct {
    food_category_t category;       // 类别编号
    const char *name;               // 类别名称（中文）
    const char *icon;               // 图标字符
    uint16_t default_shelf_life;    // 默认保质期（天）
    uint8_t auto_extend_days;       // 自动延期天数
} food_category_config_t;

// 类别配置表（静态常量，存储在Flash）
// TODO: 调试完成后改回天数（乘数从 60 改回 86400）
// TODO: 后续统一更新为中文显示（需添加中文字模）
static const food_category_config_t FOOD_CATEGORY_TABLE[FOOD_CATEGORY_COUNT] = {
    { FOOD_CAT_DAIRY,   "Dairy",   "D",  7,  1 },   // 调试：7分钟（正式：7天）
    { FOOD_CAT_MEAT,    "Meat",    "M",  3,  1 },   // 调试：3分钟（正式：3天）
    { FOOD_CAT_VEG,     "Veggie",  "V",  5,  2 },   // 调试：5分钟（正式：5天）
    { FOOD_CAT_FRUIT,   "Fruit",   "F",  7,  2 },   // 调试：7分钟（正式：7天）
    { FOOD_CAT_SEAFOOD, "Seafood", "S",  2,  1 },   // 调试：2分钟（正式：2天）
    { FOOD_CAT_DRINK,   "Drink",   "K",  30, 3 },   // 调试：30分钟（正式：30天）
    { FOOD_CAT_FROZEN,  "Frozen",  "I",  90, 7 },   // 调试：90分钟（正式：90天）
};

/**
 * @brief 根据类别编号获取配置
 * @param cat 类别编号 (1-7)
 * @return 配置指针，无效编号返回NULL
 */
static inline const food_category_config_t* food_category_get_config(food_category_t cat)
{
    if (cat < 1 || cat > FOOD_CATEGORY_COUNT) {
        return NULL;
    }
    return &FOOD_CATEGORY_TABLE[cat - 1];
}

/**
 * @brief 根据按键编号获取类别配置
 * @param key_id 按键编号 (1-7对应K1-K7)
 * @return 配置指针，无效按键返回NULL
 */
static inline const food_category_config_t* food_category_from_key(uint8_t key_id)
{
    if (key_id < 1 || key_id > FOOD_CATEGORY_COUNT) {
        return NULL;
    }
    return &FOOD_CATEGORY_TABLE[key_id - 1];
}

#ifdef __cplusplus
}
#endif
