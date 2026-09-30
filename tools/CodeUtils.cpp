//
// Created by fgsqme on 2022/3/20 0020.
//

#include "CodeUtils.h"


char *CodeUtils::UTFToGBK(const QString& str) {
    /*QTextCodec *utf8 = QTextCodec::codecForName("UTF-8");
    QTextCodec::setCodecForLocale(utf8); // 设置本地编码
    QTextCodec *textCodec = QTextCodec::codecForName("gbk");
    QByteArray gb_bytes = textCodec->fromUnicode(str);
    return gb_bytes.data();*/
    return nullptr;
}
/*

string CodeUtils::Gbk2Utf8(const char *gbkStr) {
    string tempstr;
    int len = MultiByteToWideChar(CP_ACP, 0, gbkStr, -1, nullptr, 0);
    auto *wstr = new WCHAR[len];
    MultiByteToWideChar(CP_ACP, 0, gbkStr, -1, wstr, len);
    len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
    char *str = new char[len];
    memset(str, 0, len + 1);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, len, nullptr, nullptr);
    delete[] wstr;
    tempstr = str;
    delete[] str;
    return tempstr;
}
*/
/*

wstring CodeUtils::CharToWchar(const char *c, size_t m_encode) {
    wstring str;
    int len = MultiByteToWideChar(m_encode, 0, c, strlen(c), nullptr, 0);
    auto *m_wchar = new wchar_t[len + 1];
    MultiByteToWideChar(m_encode, 0, c, strlen(c), m_wchar, len);
    m_wchar[len] = '\0';
    str = m_wchar;
    delete[] m_wchar;
    return str;
}

string CodeUtils::WcharToChar(const wchar_t *wp, size_t m_encode) {
    string str;
    int len = WideCharToMultiByte(m_encode, 0, wp, wcslen(wp), nullptr, 0, nullptr, nullptr);
    char *m_char = new char[len + 1];
    WideCharToMultiByte(m_encode, 0, wp, wcslen(wp), m_char, len, nullptr, nullptr);
    m_char[len] = '\0';
    str = m_char;
    delete[] m_char;
    return str;
}
*/

int CodeUtils::getUtf8StrLen(const char *str) {
    if (!str) return 0;

    int len = (int) strlen(str);

    int ret = 0;

    for (const char *sptr = str; (sptr - str) < len && *sptr;) {
        unsigned char ch = (unsigned char) (*sptr);

        if (ch < 0x80) {
            sptr++;    // ascii
            ret++;
        } else if (ch < 0xc0) {
            sptr++;    // invalid char
        } else if (ch < 0xe0) {
            sptr += 2;
            ret++;
        } else if (ch < 0xf0) {
            sptr += 3;
            ret++;
        } else {
            // ͳһ4���ֽ�
            sptr += 4;
            ret++;
        }
    }

    return ret;
}


int CodeUtils::getGbkStrLen(const char *str) {
    const char *p = str;        //p���ں������
    while (*p)                    //���ǽ�����0�������ѭ��
    {
        if (*p < 0 && (*(p + 1) < 0 || *(p + 1) < 63))            //���ĺ������
        {
            str++;                //str�ƶ�һλ��p�ƶ��ƶ�2λ����˳��ȼ�1
            p += 2;
        } else {
            p++;                //str������p�ƶ�һλ�����ȼ�1
        }
    }
    return p - str;                //���ص�ַ֮��
}
