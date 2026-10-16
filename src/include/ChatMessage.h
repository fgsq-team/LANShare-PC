//
// 聊天消息数据模型。
//
// 设计目标：把"消息"从 QWidget 中剥离出来，变成纯数据（ChatMessage），
// 控件（ChatItemWidget）只是这份数据在某一时刻的渲染产物。这样做的好处：
//   1. 一条消息可包含多个内容元素（多张图片、多个文件、文本混排），天然可扩展；
//   2. 数据库、网络层只处理数据，不再构造/持有 QWidget，避免跨线程创建控件；
//   3. 渲染层通过元素类型分发，新增消息类型 = 新增一个元素视图，无需改动上层。
//
#ifndef LANSHARE_CHATMESSAGE_H
#define LANSHARE_CHATMESSAGE_H

#include <QString>
#include <QVector>
#include "Type.h"

// 前向声明：LFile 仅在运行期被文件元素持有，不参与序列化。
class LFile;

/**
 * 消息内容元素类型。
 * 新增类型时只需在此追加枚举，并在渲染层注册对应元素视图。
 */
enum class ElementType : int {
    Text = 0,  ///< 纯文本
    File = 1,  ///< 文件或文件夹
    Image = 2  ///< 图片（缩略图 + 点击预览）
};

/**
 * 消息气泡的朝向。左 = 对方发送，右 = 本机发送。
 */
enum class ChatDirection : int {
    Left = 0,
    Right = 1
};

/**
 * 单个内容元素。
 *
 * 采用"扁平结构 + 类型标签"而非多态继承，原因：
 *   - 便于 JSON 序列化 / 数据库存取；
 *   - 元素数量少、字段简单，多态带来的收益不及其复杂度。
 * 若未来元素字段急剧膨胀，可再拆分为基类 + 子类。
 */
struct ContentElement {
    ElementType type = ElementType::Text;

    // --- 文本 ---
    QString text;  ///< Text 元素的正文；File/Image 元素可作为展示标题（文件名）

    // --- 文件 / 图片共有 ---
    QString elementUuid;  ///< 元素级唯一 id，用于进度 / 完成状态回写定位
    QString filePath;     ///< 绝对路径
    QString name;         ///< 显示名称
    mlong size = 0;       ///< 字节大小
    QString sizeStr;      ///< 已格式化的大小字符串
    bool isDirectory = false;

    // --- 传输状态（元素级，支持一条消息多个文件各自显示进度）---
    int progress = 0;      ///< 0~100
    bool completed = false;
    bool received = false; ///< 是否已接收（区分发送/接收的失败展示文案）

    // --- 运行期持有，不参与序列化 ---
    LFile *file = nullptr;
};

/**
 * 一条完整的聊天消息。
 */
struct ChatMessage {
    QString uuid;              ///< 消息唯一 id（数据库主键）
    QString deviceName;        ///< 发送方设备名
    int devMode = -1;          ///< 设备类型（Device::L_xxx）
    ChatDirection direction = ChatDirection::Left;
    int dataVersion = 0;       ///< 协议数据版本
    QString createTime;        ///< 展示用创建时间字符串
    qint64 timeStamp = 0;      ///< 排序用时间戳

    QVector<ContentElement> elements;  ///< 内容元素列表（可多个）

    ChatMessage() = default;

    /**
     * 构造一条纯文本消息（单 Text 元素）。
     */
    static ChatMessage fromText(const QString &text, const QString &deviceName,
                                int devMode, ChatDirection direction);

    /**
     * 构造一条文件消息（单 File 元素）。
     */
    static ChatMessage fromFile(const QString &name, const QString &elementUuid,
                                const QString &deviceName, int devMode,
                                ChatDirection direction, mlong size, bool isDirectory);

    /**
     * 构造一条图片消息（单 Image 元素）。
     */
    static ChatMessage fromImage(const QString &path, const QString &name,
                                 const QString &elementUuid, const QString &deviceName,
                                 int devMode, ChatDirection direction, mlong size);

    /// 按扩展名判断文件名是否图片（各传输链路统一判定入口）。
    static bool isImageName(const QString &name);

    // --- 便捷查询 ---

    /// 是否包含任意文件/图片元素。
    bool hasMedia() const;

    /// 第一个指定类型的元素指针，找不到返回 nullptr。
    const ContentElement *firstOf(ElementType type) const;
    ContentElement *firstOf(ElementType type);

    /// 按 elementUuid 查找元素，找不到返回 nullptr。
    ContentElement *findElement(const QString &elementUuid);

    /// 用于复制/展示的主文本：优先 Text 元素，否则退化为标题/文件名。
    QString displayText() const;

    /// 收集消息内所有运行期 LFile 指针（用于取消传输）。
    QVector<LFile *> collectFiles() const;
};

#endif //LANSHARE_CHATMESSAGE_H
