<?php
class CouchbaseViewQuery {
    public $ddoc = '';
    public $name = '';
    public $options = array();

    const UPDATE_BEFORE = 1;
    const UPDATE_NONE = 2;
    const UPDATE_AFTER = 3;

    const ORDER_ASCENDING = 1;
    const ORDER_DESCENDING = 2;

    private function __construct() {
    }

    static public function from($ddoc, $name) {
        $res = new _CouchbaseDefaultViewQuery();
        $res->ddoc = $ddoc;
        $res->name = $name;
        return $res;
    }

    static public function fromSpatial($ddoc, $name) {
        $res = new _CouchbaseSpatialViewQuery();
        $res->ddoc = $ddoc;
        $res->name = $name;
        return $res;
    }

    public function stale($stale) {
        if ($stale == self::UPDATE_BEFORE) {
            $this->options['stale'] = 'false';
        } else if ($stale == self::UPDATE_NONE) {
            $this->options['stale'] = 'ok';
        } else if ($stale == self::UPDATE_AFTER) {
            $this->options['stale'] = 'update_after';
        } else {
            throw new CouchbaseException('invalid option passed.');
        }
        return $this;
    }

    public function skip($skip) {
        $this->options['skip'] = '' . $skip;
        return $this;
    }

    public function limit($limit) {
        $this->options['limit'] = '' . $limit;
        return $this;
    }

    public function custom($opts) {
        foreach ($opts as $k => $v) {
            $this->options[$k] = $v;
        }
        return $this;
    }
};

class _CouchbaseDefaultViewQuery extends CouchbaseViewQuery {
    public function __construct() {
    }

    public function order($order) {
        if ($order == self::ORDER_ASCENDING) {
            $this->options['descending'] = 'false';
        } else if ($order == self::ORDER_DESCENDING) {
            $this->options['descending'] = 'true';
        } else {
            throw new CouchbaseException('invalid option passed.');
        }
        return $this;
    }

    public function reduce($reduce) {
        if ($reduce) {
            $this->options['reduce'] = 'true';
        } else {
            $this->options['reduce'] = 'false';
        }
        return $this;
    }

    public function group($group_level) {
        if ($group_level >= 0) {
            $this->options['group'] = 'false';
            $this->options['group_level'] = '' . $group_level;
        } else {
            $this->options['group'] = 'true';
            $this->options['group_level'] = '0';
        }
        return $this;
    }

    public function key($key) {
        $this->options['key'] = $key;
        return $this;
    }

    public function keys($keys) {
        $this->options['keys'] =
            str_replace('\\\\', '\\', json_encode($keys));
        return $this;
    }

    public function range($start = NULL, $end = NULL, $inclusive_end = false) {
        if ($start !== NULL) {
            $this->options['startkey'] =
                str_replace('\\\\', '\\', json_encode($start));
        } else {
            $this->options['startkey'] = '';
        }
        if ($end !== NULL) {
            $this->options['endkey'] =
                str_replace('\\\\', '\\', json_encode($end));
        } else {
            $this->options['endkey'] = '';
        }
        $this->options['inclusive_end'] = $inclusive_end ? 'true' : 'false';
        return $this;
    }

    public function id_range($start = NULL, $end = NULL) {
        if ($start !== NULL) {
            $this->options['startkey_docid'] =
                str_replace('\\\\', '\\', json_encode($start));
        } else {
            $this->options['startkey_docid'] = '';
        }
        if ($end !== NULL) {
            $this->options['startkey_docid'] =
                str_replace('\\\\', '\\', json_encode($end));
        } else {
            $this->options['startkey_docid'] = '';
        }
        return $this;
    }
};

class _CouchbaseSpatialViewQuery extends CouchbaseViewQuery {
    public function __construct() {
    }

    public function bbox($bbox) {
        $this->options['bbox'] = implode(',', $bbox);
        return $this;
    }
};
