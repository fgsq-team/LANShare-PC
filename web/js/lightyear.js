var lightyear = function () {
    var pageLoader = function ($mode) {
        var $loadingEl = jQuery('#lyear-loading');
        $mode = $mode || 'show';
        if ($mode === 'show') {
            if ($loadingEl.length) {
                $loadingEl.fadeIn(250)
            } else {
                jQuery('body').prepend('<div id="lyear-loading"><div class="spinner-border text-primary" role="status"><span class="sr-only">Loading...</span></div></div>')
            }
        } else if ($mode === 'hide') {
            if ($loadingEl.length) {
                $loadingEl.fadeOut(250)
            }
        }
        return false
    };
    var tips = function ($msg, $type, $delay, $icon, $from, $align, $url) {
        $type = $type || 'info';
        $delay = $delay || 1000;
        $from = $from || 'top';
        $align = $align || 'center';
        $enter = $type == 'danger' ? 'animated shake' : 'animated fadeInUp';
        $url = $url || url;
        jQuery.notify({icon: $icon, message: $msg}, {
            element: 'body',
            type: $type,
            allow_dismiss: true,
            newest_on_top: true,
            showProgressbar: false,
            placement: {from: $from, align: $align},
            offset: 20,
            spacing: 10,
            z_index: 10800,
            delay: $delay,
            animate: {enter: $enter, exit: 'animated fadeOutDown'}
        });
        if ($url != '') {
            setTimeout(function () {
                window.location.href = $url
            }, $delay)
        }
    };
    var url = '';
    return {
        notify: function ($msg, $type, $delay, $icon, $from, $align, $url) {
            tips($msg, $type, $delay, $icon, $from, $align, $url)
        }, url: function ($url) {
            url = $url
        }, loading: function ($mode) {
            pageLoader($mode)
        }
    }
}();