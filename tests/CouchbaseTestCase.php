<?php
class CouchbaseTestCase extends PHPUnit_Framework_TestCase {
    public $testHost;
    public $testBucket;
    public $testUser;
    public $testPass;
    
    public function __construct() {
        $this->testHost = getenv('CPHOST');
        if ($this->testHost === FALSE) {
            $this->testHost = 'localhost:8091';
        }
        
        $this->testBucket = getenv('CPBUCKET');
        if ($this->testBucket === FALSE) {
            $this->testBucket = '';
        }
        
        $this->testUser = getenv('CPUSER');
        if ($this->testUser === FALSE) {
            $this->testUser = '';
        }
        
        $this->testPass = getenv('CPPASS');
        if ($this->testPass === FALSE) {
            $this->testPass = '';
        }
    }
    
    function makeKey($prefix) {
        return uniqid($prefix);
    }

    function assertValidMetaDoc($metadoc) {
        // Note that this is only valid at the moment, in the future
        //   a MetaDoc might be a simple array, or other.
        $this->assertInstanceOf('CouchbaseMetaDoc', $metadoc);

        // Check it has all the fields it should.
        for ($i = 1; $i < func_num_args(); ++$i) {
            $attr = func_get_arg($i);
            $this->assertObjectHasAttribute($attr, $metadoc);
        }
    }

    function assertErrorMetaDoc($metadoc, $type, $code) {
        $this->assertValidMetaDoc($metadoc, 'error');

        $this->assertInstanceOf($type, $metadoc->error);
        $this->assertEquals($code, $metadoc->error->getCode());
    }

    function wrapException($cb, $type = NULL, $code = NULL) {
        PHPUnit_Framework_Error_Notice::$enabled = false;
        $exOut = NULL;
        try {
            $cb();
        } catch (Exception $ex) {
            $exOut = $ex;
        }
        PHPUnit_Framework_Error_Notice::$enabled = true;

        if ($type !== NULL) {
            $this->assertErrorType($type, $exOut);
        }
        if ($code !== NULL) {
            $this->assertErrorCode($code, $exOut);
        }

        return $exOut;
    }

    function assertError($type, $code, $ex) {
        $this->assertErrorType($type, $ex);
        $this->assertErrorCode($code, $ex);
    }

    function assertErrorType($type, $ex) {
        $this->assertInstanceOf($type, $ex);
    }

    function assertErrorCode($code, $ex) {
        $this->assertEquals($code, $ex->getCode());
    }
}