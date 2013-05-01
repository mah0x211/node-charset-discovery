{
    'targets': [
        {
            'target_name': 'charset-discovery',
            'sources': [
                'src/charset_discovery.cc'
            ],
            'cflags': [
                '<!(pkg-config icu-i18n --cflags)'
            ],
            'libraries': [
                '<!(pkg-config icu-i18n --libs)'
            ]
        }
    ]
}
