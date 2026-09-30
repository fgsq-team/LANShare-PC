let fastFile = {
    path: "/",
    isFile: false,
    name: "...",
    isDirectory: true
};

let files = [fastFile, {
    path: "",
    isFile: false,
    name: "/"
}];

let medias = [];
let rootPath = "/";
let apps = [];
//  在第几个标签页
let tabPage = 1;

initConfig();

function truncateString(str, maxLength) {
    if (str.length > maxLength) {
        return str.slice(0, maxLength - 3) + '...';
    }
    return str;
}

function showUploadDialog() {
    $('#select-file').val('');
    $('#displayFile').val('');
    $("#uploadFileDialog").modal('show');
}

function uploadFile(data, success, fail) {
    let ajax = new XMLHttpRequest();
    ajax.open("POST", "/uploadFile", true);
    ajax.setRequestHeader('token', localStorage.getItem('token'));
    ajax.upload.addEventListener("progress", function(ev) {
        // $('.upload-btn').text(parseInt(ev.loaded / ev.total * 100) + "%")
    }, false)
    ajax.send(data)
    ajax.onreadystatechange = function() {
        if (ajax.readyState === 4) {
            if (ajax.status >= 200 && ajax.status < 300 || ajax.status === 304) {
                success(ajax.responseText)
            } else if (ajax.status >= 302) {
                window.location.href = ajax.getResponseHeader("Location");
            } else if (ajax.status === 500) {
                fail(ajax.responseText);
            } else {
                fail("上传文件失败");
            }
        }
    }
}

function showContextMenu(x, y) {
    var contextMenu = $('.context-menu');
    contextMenu.css({display: 'block', left: x + 'px', top: y + 'px'});
    // 点击菜单项时隐藏菜单
    contextMenu.on('click', function () {
        hideContextMenu();
    });
}

function hideContextMenu() {
    var contextMenu = $('.context-menu');
    contextMenu.css('display', 'none');
}

$(function () {
    let selector = $('#select-file');
    $('#displayFile').on('change', function (event) {
        $('#select-file').val($('#displayFile').val());
        const fileInput = event.target;
        const file = fileInput.files[0];
        if (file) {
            uploadData = new FormData();
            uploadData.append('file', file);
            var selectSendDevice = $(".select_send_device");
            selectSendDevice.empty();
            selectSendDevice.append("<tr onclick='startSendFile(null)' class=\"select_send_device_item\"><td>本机设备</td><td></td></tr>\n")
            for (let i = 0; i < onLineDeviceList.length; i++) {
                selectSendDevice.append("<tr onclick='startSendFile(\"" + onLineDeviceList[i].address + "\")' class=\"select_send_device_item\"><td>"
                    + onLineDeviceList[i].devName
                    + "</td><td>" + onLineDeviceList[i].devIP + "</td></tr>\n")
            }
            $("#uploadFileDialog").modal('hide');
            $("#devSelectDialog").modal('show');
        }
    });

    $(document).on('dragover', function (ev) {
        selector.css('display', 'block');
        ev.preventDefault();
    });
    selector.on('dragenter', function () {
        selector.attr('placeholder', '松手即可上传');
    });

    selector.on('dragleave', function () {
        selector.attr('placeholder', '拖拽到此处上传文件');
    });

    selector.on('drop', function (ev) {
        selector.attr('placeholder', '点击选择文件或者拖拽文件到此处');
        $('#uploadFileDialog').modal('hide');
        // $("#chatUploadModal").modal('hide');
        uploadData = new FormData();
        Array.from(ev.originalEvent.dataTransfer.files).forEach(function(file){
            uploadData.append('file', file)
        });
        var selectSendDevice = $(".select_send_device");
        selectSendDevice.empty();
        selectSendDevice.append("<tr onclick='startSendFile(null)' class=\"select_send_device_item\"><td>本机设备</td><td></td></tr>\n")
        for (let i = 0; i < onLineDeviceList.length; i++) {
            selectSendDevice.append("<tr onclick='startSendFile(\"" + onLineDeviceList[i].address + "\")' class=\"select_send_device_item\"><td>"
                + onLineDeviceList[i].devName
                + "</td><td>" + onLineDeviceList[i].devIP + "</td></tr>\n")
        }
        $("#uploadFileDialog").modal('hide');
        $("#devSelectDialog").modal('show');
        ev.preventDefault();
    });
    // 右键菜单
    $(document).on('contextmenu', function (e) {
        // 阻止默认右键菜单
        e.preventDefault();
        // 检查右键事件发生的目标元素是否包含特定类名
        if ($(e.target).hasClass('media-file')) {
            let fileName = $(e.target).attr('file-name');
            let path = $(e.target).attr('path');
            let contextMenu = $(".context-menu");
            let requestUrl = window.location.origin + "/file/" + fileName + "?path=" + encodeURIComponent(path) + "&token=" + localStorage.getItem("token");
            contextMenu.empty();
            contextMenu.append('<a class="menu-item" onclick="openFile(\'' + fileName + '\',\'' + path + '\')">打开</a>');
            contextMenu.append('<a class="menu-item" onclick="copyToClip(\'' + requestUrl + '\')">复制链接</a>');
            // 显示自定义右键菜单
            showContextMenu(e.clientX, e.clientY);
        } else if ($(e.target).hasClass('media-dir')) {
            let index = $(e.target).attr('m-index');
            let contextMenu = $(".context-menu");
            contextMenu.empty();
            contextMenu.append('<a class="menu-item" onclick="openHash(' + index + ')">打开</a>');
            // 显示自定义右键菜单
            showContextMenu(e.clientX, e.clientY);
        }
    });
    $(document).on('click', function () {
        // 点击页面任何地方时隐藏右键菜单
        hideContextMenu();
    });
    // 媒体
    $(".nav-item-media").click(function () {
        window.location.hash = 'nav-item-media?index=-1';
    });
    //  文件列表
    $(".nav-item-files").click(function () {
        window.location.hash = 'nav-item-files' + rootPath + '?isBack=false';
    });
    // app列表
    $(".nav-item-apps").click(function () {
        window.location.hash = 'nav-item-apps';
    });
    // 消息记录
    $(".nav-item-chat").click(function () {
        window.location.hash = 'nav-item-chat';
    });
    $(".media-img-select").click(function (e) {
        e = window.event || e;
        if (e.stopPropagation) {
            e.stopPropagation();
        } else {
            e.cancelBubble = true;
        }
    });
    $(".file-icon-select").click(function (e) {
        e = window.event || e;
        if (e.stopPropagation) {
            e.stopPropagation();
        } else {
            e.cancelBubble = true;
        }
    });
    // 路径变化监听
    window.onhashchange = locationHashChanged
});

function checkPass() {
    const timerId = setInterval(function () {
        post({
            url: "/checkPass",
            data: {},
            success: function (result) {
                if (result.pass) {
                    clearInterval(timerId); // 停止定时器
                    location.reload();
                }
            },
            error: function (result) {
                console.error(result.message);
            }
        });

    }, 2000);
}

function initConfig() {
    post({
        url: "/initConfig",
        data: JSON.stringify({"test": 1}),
        success: function (result) {
            rootPath = result.rootPath;
            localStorage.setItem("token", result.token);
            if (result.pass) {
                $('.lyear-layout-web').show();
                initWS();
                initView();
            } else {
                $("#myModal").modal('show');
                checkPass();
            }
        },
        error: function (result) {
            console.error(result.message)
        }
    });
}

function initView() {
    locationHashChanged();
}

function openFile(name, path) {
    let requestUrl = "/file/" + name + "?path=" + encodeURIComponent(path) + "&token=" + localStorage.getItem("token");
    window.open(requestUrl, "_blank");
}

/**
 * 复制单行内容到粘贴板
 * content : 需要复制的内容
 * message : 复制完后的提示，不传则默认提示"复制成功"
 */
function copyToClip(content) {
    var aux = document.createElement("input");
    aux.setAttribute("value", content);
    document.body.appendChild(aux);
    aux.select();
    document.execCommand("copy");
    document.body.removeChild(aux);
    lightyear.notify("复制成功", 'success', 1000);
}

function openApkFile(name, packageName) {
    let requestUrl = "/apkfile/" + name + "?packageName=" + packageName + "&token=" + localStorage.getItem("token");
    window.open(requestUrl, "_blank");
}

function openMedia(v) {
    if (filesSelectCount > 0) {
        let select = $(v).children(".media-img-select");
        select.prop("checked", !select.is(':checked'));
        mediaSelect(select)
    } else {
        openFile($(v).attr('file-name'), $(v).attr('path'))
    }
}

function openMediaFolder(v, index) {
    if (filesSelectCount > 0) {
        let select = $(v).children(".media-img-select");
        select.prop("checked", !select.is(':checked'));
        mediaSelect(select);
    } else {
        openHash(index);
    }
}

function openHash(index) {
    window.location.hash = 'nav-item-media?index=' + index;
}

function handleMediaIntersection(entries, observer) {
    entries.forEach(function(entry) {
        if (entry.isIntersecting) {
            const image = $(entry.target).find('.media-img');
            image.css("background-image", "url('" + image.data('url') + "')");
            observer.unobserve(entry.target); // 停止观察已经进入视口的元素
        }
    });
}

function handleApplistIntersection(entries, observer) {
    entries.forEach(function(entry) {
        if (entry.isIntersecting) {
            const image = $(entry.target).find('.app-img');
            image.css("background-image", "url('" + image.data('url') + "')");
            observer.unobserve(entry.target); // 停止观察已经进入视口的元素
        }
    });
}

const options = {
    root: null,
    rootMargin: '0px',
    threshold: 0.5, // 当目标元素50%进入视口时触发
};

// 图片懒加载
const mediaObserver = new IntersectionObserver(handleMediaIntersection, options);
const appListObserver = new IntersectionObserver(handleApplistIntersection, options);

function mediaList(index) {
    $(".media-content").empty();
    post({
        url: "/media",
        type: "post",
        data: JSON.stringify({folderIndex: index}),
        success: function (result) {
            medias = result;
            result.forEach(function (item, index) {
                let cardBox;
                if (item.isDirectory) {
                    cardBox = $("<div is-directory='true' m-index='" + item.index + "' class=\"media-dir cardBox media-item\" onclick=\"openMediaFolder(this," + item.index + ")\"></div>");
                } else {
                    cardBox = $("<div is-directory='false' m-index='" + item.index + "' i-index='" + item.subIndex + "' file-name='" + item.name + "' path='" + item.path + "'  class=\"media-file cardBox media-item\" onclick=\"openMedia(this)\"></div>");
                }
                cardBox.append($("<div class=\"media-img\" data-url='/imageload/"
                    + item.name + "?index=" + item.imgIndex + "&token=" + localStorage.getItem('token') + "'></div>"));
                cardBox.append("<input name=\"checkbox\" value=\"0\" type=\"checkbox\" onclick='event.cancelBubble=true;mediaSelect(this)' " +
                    " class=\"media-img-select\">");
                if (item.isVideo) {
                    cardBox.append($("<div class=\"media-video-time\">" + item.videoTime + "</div>"));
                }
                if (item.isDirectory) {
                    cardBox.append("<div>" + truncateString(item.name, 18) + "</div>");
                }
                $(".media-content").append(cardBox);
                mediaObserver.observe(cardBox[0]);
            });
        },
        error: function (result) {
            console.error(result.message);
        }
    });
}

var filesSelectCount = 0;

function mediaSelect(v) {
    if ($(v).is(':checked')) {
        filesSelectCount++;
    } else {
        filesSelectCount--
    }
    if (filesSelectCount > 0) {
        $('.download-files').show();
    } else {
        $('.download-files').hide();
    }
    $('.download-files').text('下载文件(' + filesSelectCount + ')');
}

function fileSelect(v) {
    if ($(v).is(':checked')) {
        filesSelectCount++;
    } else {
        filesSelectCount--;
    }
    if (filesSelectCount > 0) {
        $('.download-files').show();
    } else {
        $('.download-files').hide();
    }
    $('.download-files').text('下载文件(' + filesSelectCount + ')');
}


function resetMediaSelect() {
    filesSelectCount = 0;
    $('.download-files').hide();
}

function appList() {
    $(".app-content").empty();
    post({
        url: "/apps",
        data: JSON.stringify({}),
        success: function (result) {
            apps = result.list;
            apps.forEach(function (item, index) {
                let appbox = $("<div class='appBox media-item' onclick='openApkFile(\"" + item.name + "\",\"" + item.packageName + "\")'></div>");
                appbox.append($("<div class='app-img' data-url='/appicon?packageName=" + item.packageName + "'/>"))
                appbox.append($("<div>" + item.name + "</div>"))
                appbox.append($("<div>" + item.length + "</div>"))
                appbox.append($("<img class='app-select' src='/drawable?name=ic_image_un_select'/>"))
                $(".app-content").append(appbox);
                appListObserver.observe(appbox[0]);
            });
        },
        error: function (result) {
            // console.error(result);
        }
    });
}

function dirClick(isBack, path) {
    window.location.hash = 'nav-item-files' + path + "?isBack=" + isBack;
}

function openDir(isBack, path) {
    post({
        url: "/files",
        data: JSON.stringify({path: path, isBack: isBack}),
        success: function (result) {
            viewEmpty();
            fastFile.path = result.path;
            // 设置路径显示
            $('.path-text').text(result.path);
            // 文件列表
            files = result.list;
            // // 插入首行的返回按钮
            // files.splice(0, 0, fastFile);
            let i = 0;
            // 循环遍历上面的json数据，每一行代表一个li
            files.forEach(function (item, index) {
                let li;
                if (item.isDirectory) {
                    li = $("<div file-path='" + item.path + "' class='file-item' onclick=\"dirClick('" + (index === 0) + "',encodeURI($(this).attr('file-path')))\"></div>");
                } else {
                    li = $("<div file-path='" + item.path + "'  class='file-item' onclick=\"openFile('" + item.name + "',$(this).attr('file-path'))\"></div>");
                }
                if (index !== 0) {
                    li.append("<input name=\"checkbox\" value=\"0\" type=\"checkbox\" onclick=\"event.cancelBubble=true;fileSelect(this)\" class=\"file-img-select\">");
                }
                if (item.isDirectory) {
                    li.append("<div class='file-item-icon file-item-base file-icon-folder'></div>");
                } else {
                    li.append("<div class='file-item-icon file-item-base file-icon-file'></div>");
                }
                let info = $('<div class="file-item-info"></div>');
                info.append($("<div class='file-item-name'>" + item.name + "</div>"));
                if (index === 0) {
                    info.append($("<div class='file-item-time file-item-base'></div>"));
                } else {
                    info.append($("<div class='file-item-time file-item-base'>" + item.time + "</>"));
                }
                li.append(info);
                $(".filelist").append(li);
                i++
            });
        },
        error: function (result) {
            console.log(result.message);
        }
    });
}

function viewEmpty() {
    resetMediaSelect();
    $(".filelist").empty();
    $(".media-content").empty();
    $(".app-content").empty();
    // $(".chat-content").empty();
    // 拖动条回到顶部
    document.body.scrollTop = document.documentElement.scrollTop = 0;
}

function downloadMedia() {
    list = [];
    $('.media-item').each(function (index, dom) {
        if ($(dom).find('.media-img-select').is(':checked')) {
            isDirectory = $(dom).attr("is-directory");
            index = -1;
            subIndex = -1;
            if (isDirectory === 'true') {
                index = $(dom).attr("m-index");
            } else {
                index = $(dom).attr("m-index");
                subIndex = $(dom).attr("i-index");
            }
            list.push({
                index: index - 0,
                subIndex: subIndex - 0,
                isDirectory: isDirectory === 'true'
            })
        }
    });
    lightyear.notify("文件正在打包中...", 'info', 1000);
    lightyear.loading('show');
    post({
        url: "/compressMedias",
        data: JSON.stringify({"list": list}),
        success: function (result) {
            lightyear.loading('hide');
            lightyear.notify("文件打包成功，开始下载", 'success', 1000);
            window.location.href = "/downloadZipFile/" + result.tempFile + "?tempFile=" + result.tempFile
        },
        error: function (result) {
            console.error(result.message)
            lightyear.loading('hide');
            lightyear.notify("打包文件失败", 'danger', 1000);
        }
    });
}

function downloadFile() {
    list = []
    $('.file-item').each(function (index, dom) {
        if ($(dom).find('.file-img-select').is(':checked')) {
            filePath = $(dom).attr("file-path")
            list.push(filePath)
        }
    });
    lightyear.notify("文件正在打包中...", 'info', 1000);
    lightyear.loading('show');
    post({
        url: "/compressFiles",
        data: JSON.stringify({"list": list}),
        success: function (result) {
            lightyear.loading('hide');
            lightyear.notify("文件打包成功，开始下载", 'success', 1000);
            window.location.href = "/downloadZipFile/" + result.tempFile + "?tempFile=" + result.tempFile;
        },
        error: function (result) {
            console.error(result.message);
            lightyear.loading('hide');
            lightyear.notify("打包文件失败", 'danger', 1000);
        }
    });
}

function downloadFiles() {
    if (tabPage === 1) {
        downloadFile();
    } else if (tabPage === 2) {
        downloadMedia();
    }
}

function locationHashChanged() {
    let url = window.location.hash.substring(1);
    if (url.startsWith('nav-item-apps')) {
        tabPage = 0;
        viewEmpty();
        $(".app-content").show();
        $(".chat-content").hide();
        $(".file-content").hide();
        $(".media-content").hide();
        appList();
    } else if (url.startsWith('nav-item-files')) {
        tabPage = 1;
        viewEmpty();
        $(".app-content").hide();
        $(".chat-content").hide();
        $(".file-content").show();
        $(".media-content").hide();
        var p = "nav-item-files";
        var path = url.substring(p.length, url.indexOf("?"));
        var isBack = url.substring(url.lastIndexOf("=") + 1) === "true";
        openDir(isBack, decodeURIComponent(path));
    } else if (url.startsWith('nav-item-media')) {
        tabPage = 2;
        let indexParams = url.substring(url.indexOf("?") + 1);
        let index = indexParams.split("=")[1] - 0;
        viewEmpty();
        $(".chat-content").hide();
        $(".app-content").hide();
        $(".file-content").hide();
        $(".media-content").show();
        mediaList(index);
    } else /*if (url.startsWith('nav-item-chat'))*/ {
        tabPage = 3;
        viewEmpty();
        $(".app-content").hide();
        $(".file-content").hide();
        $(".media-content").hide();
        $(".chat-content").show();
    }
}

