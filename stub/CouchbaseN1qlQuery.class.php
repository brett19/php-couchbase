<?php
/**
 * File for the CouchbaseN1qlQuery class.
 */

/**
 * Represents a N1QL query to be executed against a Couchbase bucket.
 *
 * @package Couchbase
 */
class CouchbaseN1qlQuery {
    /**
     * @var string
     * @internal
     */
    public $querystr = '';

    /**
     * Creates a new N1qlQuery instance directly from a N1QL DML.
     * @param $str
     * @return CouchbaseNq1lQuery
     */
    static public function fromString($str) {
        $res = new CouchbaseNq1lQuery();
        $res->querystr = $str;
        return $res;
    }

    /**
     * Generates the N1QL string as it will be passed to the server.
     *
     * @return string
     */
    public function toString() {
        return $this->querystr;
    }
}
