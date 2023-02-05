const smartChat = function() {
  const ESP_HOST = location.origin;

  const TEXTAREA_MAN_ROWS_PORTRAIT = IS_MOBILE ? 6 : 10,
        TEXTAREA_MAX_ROWS_LANDSCAPE = 2,
        TEXT_KEYBOARD_OPEN_DURATION = 150,
        MAX_CHAT_HISTORY_MESSAGES = IS_MOBILE ? 30 : 50,
        HISTORY_CLEAR_ANIMATE_N_LAST_MESSAGES = 50,
        CHAT_MIN_WIDTH = 350,
        CHAT_MAX_WIDTH = -1, // -1 means as 100vw  // Desktop only
        SCROLL_DELTA = 25;

  const UPDATES_MAX_PING_TIMEOUT = 4000,  // In ms
        UPDATES_RECONNECTION_TIMEOUT = 500,
        UPDATES_ESTABLISH_TIME = 3000;

  const $body = document.querySelector('body'),
        $chatBox = document.querySelector('#chat-box'),
        $sendButton = document.querySelector('#send-button'),
        $messagesArea = document.querySelector('#messages-area'),
        $chatFooter = document.querySelector("#chat-footer"),
        $textarea = document.querySelector("#chat-textarea"),
        $clearHistoryButton = document.querySelector("#clean-history-button"),
        $chatResizeColumn = document.querySelector('#chat-resize'),
        $showMarkupMenuButton = document.querySelector('#toggle-keyboard-markup'),
        $optionButton = document.querySelector('#chat-option'),
        $keyboardMarkupButtons = document.querySelectorAll('#keyboard-menu button'),
        $keyboardMarkup = document.querySelector('#keyboard-menu'),
        $metaTheme = document.querySelector('meta[name="theme-color"]'),
        $activityStatus = document.querySelector('#activity-status');

  let isTextKeyboardOpened = false,
      viewportHeight = window.innerHeight,
      keyboardIntervalId = null,
      doNotBlurTextarea = false,
      getUpdates = null,
      lastPongTimeoutId = null;

  const getNumberFromStyle = (declaration, units) => Number(declaration.slice(0, declaration.length - units.length));

  const scrollToBodyBottom = () => window.scrollTo(0, document.body.scrollHeight),
        updateChatBoxHeight = () => $chatBox.style.maxHeight = window.innerHeight + 'px',
        getViewportWidth = () => Math.max(document.documentElement.clientWidth || 0, window.innerWidth || 0);

  const updateChatBoxWidth = () => {
    let widthValue = localStorage.getItem('chat.width');
    $chatResizeColumn.style.marginLeft = (
      widthValue > CHAT_MAX_WIDTH && CHAT_MAX_WIDTH != -1
        ? getViewportWidth() - CHAT_MAX_WIDTH
        : getViewportWidth() - widthValue
    ) + 'px';
  };

  const TEXTAREA_HEIGHT = $textarea.offsetHeight,
        TEXTAREA_COMPUTED_STYLE = getComputedStyle($textarea),
        TEXTAREA_PADDING = getNumberFromStyle(TEXTAREA_COMPUTED_STYLE.paddingTop, "px") + getNumberFromStyle(TEXTAREA_COMPUTED_STYLE.paddingBottom, "px"),
        TEXTAREA_ONE_ROW_HEIGHT = TEXTAREA_COMPUTED_STYLE.lineHeight.slice(0, TEXTAREA_COMPUTED_STYLE.lineHeight.length - 2),
        FOOTER_PADDING = IS_MOBILE ? 12 : 28;

  const getRowsCount = scroll => (scroll - TEXTAREA_PADDING) / TEXTAREA_ONE_ROW_HEIGHT,
        scrollHeightToEnd = rows => rows * TEXTAREA_ONE_ROW_HEIGHT + TEXTAREA_PADDING;

  const setButtonOption = isSendOption => isSendOption
                                            ? $optionButton.classList.add('chat__current-option_send')
                                            : $optionButton.classList.remove('chat__current-option_send');

  const getLastChatMessage = () => $messagesArea.querySelector('.body-messages__message'),
        setTextareaValue = value => ($textarea.value = value) && resizeTextareaRows(),
        clearKeyboardLanguageChange = () => clearInterval(keyboardIntervalId);

  const childOfByClassName = ($child, className) => {
    while(($child=$child.parentNode) && !$child?.classList?.contains(className));
    return $child;
  };

  const toggleKeyboardMarkup = state => {
    localStorage.setItem('chat.keyboard.state', state);
    state
      ? $keyboardMarkup.classList.add('chat__keyboard_opened')
      : $keyboardMarkup.classList.remove('chat__keyboard_opened')
  };

  const resizeTextareaRows = () => {
    setButtonOption($textarea.value.trim() != '');

    $textarea.style.height = TEXTAREA_HEIGHT + 'px';

    let maxRows = TEXTAREA_MAN_ROWS_PORTRAIT;
    const scroll = $textarea.scrollHeight,
        rows = getRowsCount(scroll);

    if(IS_MOBILE)
      switch(window.orientation) {
        case 90: case -90: {
          maxRows = TEXTAREA_MAX_ROWS_LANDSCAPE;
          break;
        };
      };

    if(scroll > TEXTAREA_HEIGHT && document.activeElement == $textarea)
      toggleKeyboardMarkup(false);

    if(rows <= maxRows) {
      $textarea.style.overflow = 'hidden';
      $textarea.style.height = scroll + 'px';
      $chatFooter.style.minHeight = scroll+FOOTER_PADDING + 'px';
    }
    else {
      const height = scrollHeightToEnd(maxRows);
      $textarea.style.height = height + 'px';
      $chatFooter.style.minHeight = height+FOOTER_PADDING + 'px';
      $textarea.style.overflow = 'auto';
    };
  };

  const resizeChatBoxHeight = () => {
    setTimeout(() => {
      resizeTextareaRows();
      scrollToBodyBottom();
      updateChatBoxHeight();
    }, 50)
  };

  const clearChatHistory = () => {
    const lastAnimatedMessages = $messagesArea.querySelectorAll('.body-messages__message_animate');
    lastAnimatedMessages.forEach(item => {
      item.classList.remove('body-messages__message_animate')
    })

    setTimeout(() => {
      const chatMessages = $messagesArea.querySelectorAll(`.body-messages__message:nth-child(-n+${HISTORY_CLEAR_ANIMATE_N_LAST_MESSAGES})`);
      chatMessages.forEach($message => $message.classList.add('body-messages__message_remove'));
    }, 0);
    setTimeout(() => $messagesArea.innerHTML = '', 400);
  };

  const getStringBrowserTime = () => {
    const date = new Date(),
          hours = (date.getHours()<10?'0':'') + date.getHours(),
          minutes = (date.getMinutes()<10?'0':'') + date.getMinutes();

          return `${hours}:${minutes}`;
  };

  const addNewChatMessage = (message, className) => {
    const commandRegex = /(^\/|[^\/\w<>][\/])\w+(?!\/)(?!\w)/g;
    let handledMessage = message.trim().replace(commandRegex, message => {
      const notSlashCharacter = !message.startsWith('/') ? message.slice(0, 1) : '',
            command = !!notSlashCharacter ? message.slice(1): message;
      return `${notSlashCharacter}<span class=\"body-messages__command\">${command}</span>`;
    });

    const messageTemplate = `
      <div class="body-messages__message ${className}">
          <div class="body-messages__container">
              <svg class="body-messages__corner" viewBox="0 0 11 20">
                  <path d = "M-6 17h6v17c-.193-2.84-.876-5.767-2.05-8.782-.904-2.325-2.446-4.485-4.625-6.48A1 1 0 01-6 16z" />
              </svg>
              <div class="body-messages__wrapper">
                  <div class="body-messages__data">
                      <span class="body-messages__content" dir="ltr">${handledMessage}</span>
                      <div class="body-messages__info">
                          <time class="body-messages__time">${getStringBrowserTime()}</time>
                      </div>
                  </div>
              </div>
          </div>
      </div>
    `;

    if($messagesArea.childElementCount > MAX_CHAT_HISTORY_MESSAGES)
      $messagesArea.querySelector('.body-messages__message:last-child').remove();  // Clean first added message, if now total messages > MAX_CHAT_HISTORY_MESSAGES

    let lastMessage = getLastChatMessage();

    if(lastMessage?.classList.contains(className)) {
      lastMessage.querySelector('svg').style.opacity = '0';

      lastMessage.classList.remove('body-messages__message_animate');
      lastMessage.style.removeProperty('--height');
    };

    $messagesArea.insertAdjacentHTML('afterbegin', messageTemplate);
    lastMessage = getLastChatMessage();
    lastMessage.style.setProperty('--height', "-" + lastMessage.offsetHeight + "px");
    lastMessage.classList.add('body-messages__message_animate');
  };

  const sendMyMessage = message => {
    message = message.trim();
    if(!!!message)
      return;

    fetch(`${ESP_HOST}/send`, {"method": "POST", "body": JSON.stringify({"message": message})});
  };

  const preventKeyboardBlur = () => {
    if(!isTextKeyboardOpened)
      return;

    doNotBlurTextarea = true;
    $textarea.focus();
  }

  const handleTextareaInput = () => {
    preventKeyboardBlur();
    const message = $textarea.value;
    sendMyMessage(message);
    $textarea.value = '';
    resizeTextareaRows();
  };

  const documentMouseMove = event => {
    const viewportWidth = getViewportWidth(),
          maxOffset = viewportWidth - CHAT_MIN_WIDTH,
          minOffset = CHAT_MAX_WIDTH == -1 ? 0 : viewportWidth - CHAT_MAX_WIDTH;

    let xOffset = event.pageX;

    xOffset = xOffset < minOffset ? minOffset : xOffset;
    xOffset = xOffset > maxOffset ? maxOffset : xOffset;
    $chatResizeColumn.style.marginLeft = xOffset + 'px';
  };

  const documentMouseUp = () => {
    $body.style.userSelect = 'auto';
    const chatWidth = getViewportWidth() - getNumberFromStyle($chatResizeColumn.style.marginLeft, 'px');
    localStorage.setItem('chat.width', chatWidth);
    resizeTextareaRows();
    document.removeEventListener('mousemove', documentMouseMove);
    document.removeEventListener('mouseup', documentMouseUp);
  };

  const addKeyboardLanguageChange = () => {
    clearKeyboardLanguageChange();
    viewportHeight = window.innerHeight;

    keyboardIntervalId = setInterval(() => {
      if(window.innerHeight != viewportHeight) {
        viewportHeight = window.innerHeight;
        resizeChatBoxHeight();
      };
    }, 700);
  };

  const startPong = () => {
    clearTimeout(lastPongTimeoutId);

    lastPongTimeoutId = setTimeout(() => {

      $activityStatus.classList.remove('chat__avatar_online');
      setTimeout(() => getUpdates.close(), 0);
      setTimeout(() => {
        startConnection();
      }, UPDATES_RECONNECTION_TIMEOUT);
    }, UPDATES_MAX_PING_TIMEOUT);
  };

  const startConnection = () => {
    getUpdates = new EventSource(`${ESP_HOST}/updates`);

    setTimeout(startPong, UPDATES_ESTABLISH_TIME);

    getUpdates.addEventListener('open', () => {
      $activityStatus.classList.add('chat__avatar_online')
    });

    getUpdates.addEventListener('ping', () => {
      startPong();
    });

    getUpdates.addEventListener('newUserMessage', (message) => {
        addNewChatMessage(JSON.parse(message.data).message, 'body-messages__message_mine');
        $messagesArea.scrollTo(0, 0);
    });

    getUpdates.addEventListener('newMegaMessage', (message) => {
      addNewChatMessage(message.data, 'body-messages__message_others');
    });
  };

  const initDesktop = () => {
    document.addEventListener('keydown', event => {
      if(event.key === 'ArrowUp' || event.key === 'ArrowDown') {
        $messagesArea.scrollTop += (event.key == 'ArrowUp' ? SCROLL_DELTA : -SCROLL_DELTA)
        return event.preventDefault();
      };

      if(document.activeElement != $textarea)
        return;

      if(event.shiftKey || event.key != 'Enter')
        return;

        handleTextareaInput();
        event.preventDefault();
    });

    $chatResizeColumn.addEventListener('mousedown', () => {
      document.addEventListener('mouseup', documentMouseUp);
      document.addEventListener('mousemove', documentMouseMove);
      $body.style.userSelect = 'none';
    });

    document.addEventListener('dblclick', event => {
      if(!event.target?.classList.contains('body-messages__time'))  // body-messages__time
        return;

      const $parentElement = childOfByClassName(event.target, 'body-messages__message_mine');
      if(!!!$parentElement)
        return;

      window.getSelection().empty();
      setTextareaValue($parentElement.querySelector('.body-messages__content').innerText);
    });

    window.addEventListener('resize', () => updateChatBoxWidth() || resizeTextareaRows());
    updateChatBoxWidth();

    $messagesArea.addEventListener("wheel", event => {
      event.preventDefault();
      $messagesArea.scrollTop -= event.deltaY;
    });
  };

  const initMobile = () => {
    window.addEventListener('orientationchange', resizeChatBoxHeight);

    $textarea.addEventListener('focus', () => {
      isTextKeyboardOpened = true;
      toggleKeyboardMarkup(false);
      scrollToBodyBottom();
      updateChatBoxHeight();
      addKeyboardLanguageChange();
    });

    $textarea.addEventListener('blur', () => setTimeout(() => {
      if(doNotBlurTextarea) {
        doNotBlurTextarea = false
        return;
      };

      isTextKeyboardOpened = false;
      clearKeyboardLanguageChange()
      updateChatBoxHeight();
    }, TEXT_KEYBOARD_OPEN_DURATION));

    document.addEventListener('swiped-left', event => {
      const $parentElement = childOfByClassName(event.target, 'body-messages__message_mine');
      if(!!!$parentElement)
        return;
      const text = $parentElement.querySelector('.body-messages__content').innerText;
      setTextareaValue(text);
    });
  };

  const initialLoad = () => {
    IS_MOBILE ? initMobile() : initDesktop();

    resizeTextareaRows();
    $textarea.addEventListener('input', resizeTextareaRows);

    $sendButton.addEventListener('click', handleTextareaInput);
    $clearHistoryButton.addEventListener('click', () => {
      preventKeyboardBlur()
      clearChatHistory();
    });

    $keyboardMarkupButtons.forEach($button => $button.addEventListener('click', function() {
      sendMyMessage(this.innerText);
    }));

    $showMarkupMenuButton.addEventListener('click', () => toggleKeyboardMarkup(!$keyboardMarkup.classList.contains('chat__keyboard_opened')));

    $messagesArea.addEventListener('click', event => {
      if(!event.target?.classList.contains('body-messages__command'))
        return;

      preventKeyboardBlur();
      sendMyMessage(event.target.innerText);
    });

    toggleKeyboardMarkup(JSON.parse(localStorage.getItem('chat.keyboard.state')));
    startConnection();
    setTimeout(() => $body.style.opacity = '1', 300);  // Wait if any animations in progress
  };

  initialLoad();
};

smartChat();