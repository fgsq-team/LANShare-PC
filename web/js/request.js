function post(t) {
    r = {
        url: t.url,
        type: "post",
        dataType: "json",
        contentType: "json/application",
        data: t.data,
        success: function (e) {
            t.success(e)
        },
        error: function (e) {
            t.error(e)
        }
    };
    let e = localStorage.getItem("token");
    null != e && e.length > 0 && (r.headers = {token: e}), $.ajax(r)
}