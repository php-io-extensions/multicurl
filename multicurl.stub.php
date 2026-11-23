<?php

/**
 * @generate-class-entries
 */

namespace Multicurl {
    class Multi
    {
        /** @param \CurlMultiHandle $multi */
        public static function curlMultiSetopt(mixed $multi, int $option, mixed $value): int {}

        /** @param \CurlMultiHandle $multi */
        public static function curlMultiSocketAction(mixed $multi, int $s, int $evBitmask): array {}

        /** @param \CurlMultiHandle $multi */
        public static function curlMultiFdset(mixed $multi): array {}

        /** @param \CurlMultiHandle $multi */
        public static function curlMultiTimeout(mixed $multi): array {}

        /** @param \CurlMultiHandle $multi */
        public static function curlMultiPoll(mixed $multi, array $extraFds, int $timeoutMs): array {}

        /** @param \CurlMultiHandle $multi */
        public static function curlMultiWait(mixed $multi, array $extraFds, int $timeoutMs): array {}

        /** @param \CurlMultiHandle $multi */
        public static function curlMultiWakeup(mixed $multi): int {}

        /** @param \CurlMultiHandle $multi */
        public static function curlMultiAssign(mixed $multi, int $s, mixed $sockp): int {}

        public static function curlMultiStrerror(int $code): string {}

        /** @return resource */
        public static function curlSocketStream(int $s) {}
    }
}
