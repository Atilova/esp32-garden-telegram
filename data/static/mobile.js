const mobilePlatforms =  {
    Android: () => navigator.userAgent.match(/Android/i),
    BlackBerry: () => navigator.userAgent.match(/BlackBerry/i),
    iOS: () => navigator.userAgent.match(/iPhone|iPad|iPod/i),
    Opera: () => navigator.userAgent.match(/Opera Mini/i),
    Windows: () => navigator.userAgent.match(/IEMobile/i),
    any: () => mobilePlatforms.Android() || mobilePlatforms.BlackBerry() || mobilePlatforms.iOS() || mobilePlatforms.Opera() || mobilePlatforms.Windows()
};

const IS_MOBILE = !!mobilePlatforms.any();  // For any updates page reload required