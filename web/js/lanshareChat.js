var connected = false;
var onLineDeviceList = [];
var selectedDevice = '';
var selectedDeviceName = '';
var uploadData = null;

function uploadChatFile(data, address, success, fail) {
    let ajax = new XMLHttpRequest();
    ajax.open("POST", "/chatUploadFile?address=" + address, true);
    ajax.setRequestHeader('token', localStorage.getItem('token'));
    ajax.upload.addEventListener("progress", function (ev) {
        // $('.upload-btn').text(parseInt(ev.loaded / ev.total * 100) + "%")
    }, false)
    ajax.send(data)
    ajax.onreadystatechange = function () {
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

$(function () {
    let selector = document.querySelector('.chat_content');
    selector.addEventListener('dragenter', function () {
        $("#chatUploadModal").modal('show');
        uploadData = null;
    }, false);
    let chatUploadModal = document.querySelector('#chatUploadModal');
    chatUploadModal.addEventListener('dragover', function (ev) {
        ev.preventDefault()
    }, false);
    chatUploadModal.addEventListener('dragenter', function () {
        $("#chatUploadModal").modal('show');
        uploadData = null;
    }, false);
    chatUploadModal.addEventListener('dragleave', function () {
        $("#chatUploadModal").modal('hide');

    }, false);
    chatUploadModal.addEventListener('drop', function (ev) {
        ev.preventDefault()
        $("#chatUploadModal").modal('hide');
        uploadData = new FormData();
        Array.from(ev.dataTransfer.files).forEach(function (file) {
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
        $("#devSelectDialog").modal('show');
    }, false)

    $(".chat_context_edit").keydown(function (event) {
        // 检查按下的键是否是Enter键，并且同时按下了Ctrl键
        if (event.keyCode === 13 && event.shiftKey) {
            // 阻止默认的换行行为
            event.preventDefault();
            // 在光标位置插入换行符
            var cursorPosition = this.selectionStart;
            var textBeforeCursor = $(this).val().substring(0, cursorPosition);
            var textAfterCursor = $(this).val().substring(cursorPosition);
            $(this).val(textBeforeCursor + "\r\n" + textAfterCursor);
            // 更新光标位置
            this.setSelectionRange(cursorPosition + 1, cursorPosition + 1);
            // 滚动至底部
            $(this).scrollTop($(this)[0].scrollHeight);
        } else if (event.keyCode === 13 && event.ctrlKey) {
            sendMessage(true);
        } else if (event.keyCode === 13) {
            // 阻止默认的换行行为
            event.preventDefault();
            sendMessage(false);
        }
    });
});

function startSendFile(address) {
    $("#devSelectDialog").modal('hide');
    if (address == null || address.length <= 0) {
        uploadFile(uploadData, function (a) {
            lightyear.notify(a, 'success', 1000);
            uploadData = null;
        }, function (a) {
            lightyear.notify(a, 'danger', 1000);
            uploadData = null;
        })
        return;
    }
    uploadChatFile(uploadData, address, function (a) {
        lightyear.notify(a, 'success', 1000);
        uploadData = null;
    }, function (a) {
        lightyear.notify(a, 'danger', 1000);
        uploadData = null;
    })
}

function addMessage(data) {
    var item;
    if (data.isLeft) {
        var profilePhoto;
        if (data.devType === 0) {
            profilePhoto = $('<div class="chat_profile_photo_left ic_phone"></div>');
        } else {
            profilePhoto = $('<div class="chat_profile_photo_left ic_win"></div>');
        }
        var itemContent = $('<div class="chat_item_content_left"/>');
        var nick = $('<div class="chat_left_nick clearfix">' + data.devName + '</div>');
        var messageContent;
        if (data.messageType === 0) {
            var formattedMessage = data.message.replace(/\n/g, "<br>");
            // 创建一个 <div> 元素，并将格式化后的文本插入其中
            messageContent = $('<div class="chat_left_content"></div>').html(formattedMessage);
        } else if (data.messageType === 1) {
            messageContent = $('<img  src="/file/' + data.message + '?path=' + data.filePath + '&token=' + localStorage.getItem("token") + '" class="chat_left_content_img"  onclick="openFile(\'' + data.message + '\',\'' + data.filePath + '\')">')
        } else if (data.messageType === 2) { // 文件
            messageContent = $(' <div class="chat_left_file_content" onclick="openFile(\'' + data.message + '\',\'' + data.filePath + '\')">' +
                '<div class="file_icon"></div> ' +
                '<div class="file_info_content">' +
                '<div class="file_info_name">' + data.message + '</div>' +
                '<div class="file_info_size">' + data.fileSize + '</div> ' +
                '</div> ' +
                '</div>')
        }
        itemContent.append(nick)
        itemContent.append(messageContent)
        item = $('<div class="chat_left"></div>')
        item.append(profilePhoto)
        item.append(itemContent)
    } else {
        var profilePhoto;
        if (data.devType === 0) {
            profilePhoto = $('<div class="chat_profile_photo_right ic_phone"></div>');
        } else {
            profilePhoto = $('<div class="chat_profile_photo_right ic_win"></div>');
        }
        var itemContent = $('<div class="chat_item_content_right"/>');
        var nick = $('<div class="chat_right_nick clearfix">' + data.devName + '</div>');
        var messageContent;
        if (data.messageType === 0) {  // 消息
            var formattedMessage = data.message.replace(/\n/g, "<br>");
            // 创建一个 <div> 元素，并将格式化后的文本插入其中
            messageContent = $('<div class="chat_right_content"></div>').html(formattedMessage);
        } else if (data.messageType === 1) { // 媒体
            messageContent = $('<img src="/file/' + data.message + '?path=' + data.filePath + '&token=' + localStorage.getItem("token") + '" class="chat_right_content_img" onclick="openFile(\'' + data.message + '\',\'' + data.filePath + '\')">')
        } else if (data.messageType === 2) { // 文件
            messageContent = $(' <div class="chat_right_file_content" onclick="openFile(\'' + data.message + '\',\'' + data.filePath + '\')">' +
                '<div class="' + (data.isFile ? "file_icon" : "dir_file_icon") + '"></div> ' +
                '<div class="file_info_content">' +
                '<div class="file_info_name">' + data.message + '</div>' +
                '<div class="file_info_size">' + data.fileSize + '</div> ' +
                '</div> ' +
                '</div>')
        }

        itemContent.append(nick);
        itemContent.append(messageContent);
        item = $('<div class="chat_right"></div>');
        item.append(profilePhoto);
        item.append(itemContent);
    }
    var chatContent = $('.chat_content');
    chatContent.append(item);
    var ct = document.getElementById("chat_content");
    ct.scroll({top: ct.scrollHeight, behavior: "smooth"})

}

var ws = null;

function initWS() {
    ws = new WebSocket("ws://" + window.location.host + "/wss?token=" + localStorage.getItem('token'));
    ws.onopen = function () {
        connected = true;
    };
    ws.onmessage = function (evt) {
        var json = JSON.parse(evt.data)
        if (json.cmd === SEND_MSSAGE) {
            var data = {
                isLeft: json.isLeft,
                devName: json.devName,
                devType: json.devType,
                message: json.message,
                messageType: json.messageType,
                filePath: json.filePath,
                fileSize: json.fileSize,
            }
            addMessage(data);
        } else if (json.cmd === SYNC_DEVICE_LIST) {
            onLineDeviceList = json.data;
            var deviceSelector = $(".device_selector")
            deviceSelector.empty();
            deviceSelector.append("<option value=''>所有设备</option>");
            for (let i = 0; i < onLineDeviceList.length; i++) {
                if (onLineDeviceList[i].address == selectedDevice) {
                    deviceSelector.append("<option selected='selected' value='" + onLineDeviceList[i].address + "'>" + onLineDeviceList[i].devName + "</option>");
                } else {
                    deviceSelector.append("<option value='" + onLineDeviceList[i].address + "'>" + onLineDeviceList[i].devName + "</option>");
                }
            }
        }
    };

    ws.onclose = function (evt) {
        connected = false;
        lightyear.notify("与客户端连接断开！", 'danger', 1000);
        $("#notConnectedModal").modal({
            backdrop: "static",//点击空白处不关闭对话框
            show: true
        });

    };

    ws.onerror = function (evt) {
        connected = false;
        lightyear.notify("连接客户端出现错误！", 'danger', 1000);
        $("#notConnectedModal").modal({
            backdrop: "static",//点击空白处不关闭对话框
            show: true
        });
    };

}

function sendMessage(isClip) {
    if (!connected) {
        lightyear.notify("未连接客户端，请启动LANShare后刷新页面！", 'danger', 1000);
        return;
    }
    var chatContentEdit = $('.chat_context_edit');
    var message = chatContentEdit.val();
    if (message == null || message.length === 0) {
        lightyear.notify("输入不能为空", 'danger', 1000);
        return;
    }
    var devName = "所有设备";
    if (selectedDeviceName != null && selectedDeviceName.length > 0) {
        devName = selectedDeviceName;
    }
    var data = {
        cmd: SEND_MSSAGE,
        isLeft: false,
        devName: devName + " ← 我",
        devType: 0,
        message: message,
        messageType: 0,
        selectedDevice: selectedDevice,
        isClip: isClip
        // filePath: "images/cats.jpeg"
    }
    addMessage(data);
    ws.send(JSON.stringify(data));
    chatContentEdit.val("");
    chatContentEdit[0].style.height = '40px';
}

var pressTimer;
$(".send_massage").on("mousedown", function () {
    event.preventDefault();
    pressTimer = setTimeout(function () {
        sendMessage(true);
        clearTimeout(pressTimer);
        pressTimer = null;
    }, 500); // 设置长按时间，单位为毫秒
}).on("mouseup", function () {
    if (pressTimer != null) {
        clearTimeout(pressTimer);
        sendMessage(false);
    }
});

$(".device_selector").on("change", function () {
    selectedDevice = $(".device_selector").val();
    selectedDeviceName = $(".device_selector option:selected").text();
});

var btn = document.getElementById('send_massage');
btn.onmousedown = function (event) {
    event.preventDefault();
};
