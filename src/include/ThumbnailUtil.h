#ifndef THUMBNAIL_UTIL_H
#define THUMBNAIL_UTIL_H

#include <QObject>
#include <QImage>

class ThumbnailUtil {
public:
   static QImage getThumnail(const QString &filePath, int width = -1, int height = -1);
};


#endif  //THUMBNAIL_UTIL_H
