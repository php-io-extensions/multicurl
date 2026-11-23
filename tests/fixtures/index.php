<?php

declare(strict_types=1);

// Document root of the local server the suite drives transfers against.
header('X-Multicurl-Test: 1');
echo 'hello ', $_SERVER['REQUEST_URI'];
