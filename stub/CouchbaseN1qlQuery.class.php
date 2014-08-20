<?php
class CouchbaseN1qlQuery {
    public $querystr = '';

    static public function fromString($str) {
        $res = new CouchbaseNq1lQuery();
        $res->querystr = $str;
        return $res;
    }
}
