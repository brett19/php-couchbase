<?php
require_once('CouchbaseTestCase.php');

class BucketTest extends CouchbaseTestCase {

    /**
     * @test
     * Test that connections with invalid details fail.
     */
    function testBadPass() {
        $h = new CouchbaseCluster($this->testDsn);

        $this->wrapException(function() use($h) {
            $h->openBucket('default', 'bad_pass');
        }, 'CouchbaseException', 2);
    }

    /**
     * @test
     * Test that connections with invalid details fail.
     */
    function testBadBucket() {
        $h = new CouchbaseCluster($this->testDsn);

        $this->wrapException(function() use($h) {
            $h->openBucket('bad_bucket');
        }, 'CouchbaseException', 25);
    }

    /**
     * @test
     * Test that a connection with accurate details works.
     */
    function testConnect() {
        $h = new CouchbaseCluster($this->testDsn);
        $b = $h->openBucket();
        return $b;
    }

    /**
     * @test
     * Test basic upsert
     *
     * @depends testConnect
     */
    function testBasicUpsert($b) {
        $key = $this->makeKey('basicUpsert');

        $res = $b->upsert($key, 'bob');

        $this->assertValidMetaDoc($res, 'cas');

        return $key;
    }

    /**
     * @test
     * Test basic get
     *
     * @depends testConnect
     * @depends testBasicUpsert
     */
    function testBasicGet($b, $key) {
        $res = $b->get($key);

        $this->assertValidMetaDoc($res, 'value', 'cas', 'flags');
        $this->assertEquals($res->value, 'bob');

        return $key;
    }

    /**
     * @test
     * Test basic remove
     *
     * @depends testConnect
     * @depends testBasicGet
     */
    function testBasicRemove($b, $key) {
        $res = $b->remove($key);

        $this->assertValidMetaDoc($res, 'cas');

        // This should throw a not-found exception
        $this->wrapException(function() use($b, $key) {
            $b->get($key);
        }, 'CouchbaseException', COUCHBASE_KEYNOTFOUND);
    }

    /**
     * @test
     * Test multi upsert
     *
     * @depends testConnect
     */
    function testMultiUpsert($b) {
        $keys = array(
            $this->makeKey('multiUpsert'),
            $this->makeKey('multiUpsert')
        );

        $res = $b->upsert(array(
            $keys[0] => array('value'=>'joe'),
            $keys[1] => array('value'=>'jack')
        ));

        $this->assertCount(2, $res);
        $this->assertValidMetaDoc($res[$keys[0]], 'cas');
        $this->assertValidMetaDoc($res[$keys[1]], 'cas');

        return $keys;
    }

    /**
     * @test
     * Test multi get
     *
     * @depends testConnect
     * @depends testMultiUpsert
     */
    function testMultiGet($b, $keys) {
        $res = $b->get($keys);

        $this->assertCount(2, $res);
        $this->assertValidMetaDoc($res[$keys[0]], 'value', 'flags', 'cas');
        $this->assertEquals($res[$keys[0]]->value, 'joe');
        $this->assertValidMetaDoc($res[$keys[1]], 'value', 'flags', 'cas');
        $this->assertEquals($res[$keys[1]]->value, 'jack');

        return $keys;
    }

    /**
     * @test
     * Test multi remove
     *
     * @depends testConnect
     * @depends testMultiGet
     */
    function testMultiRemove($b, $keys) {
        $res = $b->remove($keys);

        $this->assertCount(2, $res);
        $this->assertValidMetaDoc($res[$keys[0]], 'cas');
        $this->assertValidMetaDoc($res[$keys[1]], 'cas');

        // This should throw a not-found exception
        $res = $b->get($keys);

        $this->assertCount(2, $res);

        // TODO: Different exceptions here might make sense.
        $this->assertErrorMetaDoc($res[$keys[0]], 'CouchbaseException', COUCHBASE_KEYNOTFOUND);
        $this->assertErrorMetaDoc($res[$keys[1]], 'CouchbaseException', COUCHBASE_KEYNOTFOUND);
    }

    /**
     * @test
     * Test basic counter operations w/ an initial value
     *
     * @depends testConnect
     */
    function testCounterInitial($b) {
        $key = $this->makeKey('counterInitial');

        $res = $b->counter($key, +1, array('initial'=>1));
        $this->assertValidMetaDoc($res, 'value', 'flags', 'cas');
        $this->assertEquals(1, $res->value);

        $res = $b->counter($key, +1);
        $this->assertValidMetaDoc($res, 'value', 'flags', 'cas');
        $this->assertEquals(2, $res->value);

        $res = $b->counter($key, -1);
        $this->assertValidMetaDoc($res, 'value', 'flags', 'cas');
        $this->assertEquals(1, $res->value);

        $b->remove($key);
    }

    /**
     * @test
     * Test counter operations on missing keys
     *
     * @depends testConnect
     */
    function testCounterBadKey($b) {
        $key = $this->makeKey('counterBadKey');

        $this->wrapException(function() use($b, $key) {
            $b->counter($key, +1);
        }, 'CouchbaseException', COUCHBASE_KEYNOTFOUND);
    }

    /**
     * @test
     * Test expiry operations on keys
     *
     * @depends testConnect
     */
    function testExpiry($b) {
        $key = $this->makeKey('expiry');

        $b->upsert($key, 'dog', array('expiry' => 1));

        sleep(2);

        $this->wrapException(function() use($b, $key) {
            $b->get($key);
        }, 'CouchbaseException', COUCHBASE_KEYNOTFOUND);
    }

    /**
     * @test
     * Test CAS works
     *
     * @depends testConnect
     */
    function testCas($b) {
        $key = $this->makeKey('cas');

        $res = $b->upsert($key, 'dog');
        $this->assertValidMetaDoc($res, 'cas');
        $old_cas = $res->cas;

        $res = $b->upsert($key, 'cat');
        $this->assertValidMetaDoc($res, 'cas');

        $this->wrapException(function() use($b, $key, $old_cas) {
            $b->replace($key, 'ferret', array('cas'=>$old_cas));
        }, 'CouchbaseException', COUCHBASE_KEYALREADYEXISTS);
    }

    /**
     * @test
     * Test Locks work
     *
     * @depends testConnect
     */
    function testLocks($b) {
        $key = $this->makeKey('locks');

        $res = $b->upsert($key, 'jog');
        $this->assertValidMetaDoc($res, 'cas');

        $res = $b->getAndLock($key, 1);
        $this->assertValidMetaDoc($res, 'value', 'flags', 'cas');

        $this->wrapException(function() use($b, $key) {
            $b->getAndLock($key, 1);
        }, 'CouchbaseException', COUCHBASE_TMPFAIL);
    }
    
    /**
     * @test
     * Test big upserts
     *
     * @depends testConnect
     */
    function testBigUpsert($b) {
        $key = $this->makeKey('bigUpsert');

        $v = str_repeat("*", 0x1000000);
        $res = $b->upsert($key, $v);

        $this->assertValidMetaDoc($res, 'cas');

        $b->remove($key);

        return $key;
    }
    
    /**
     * @test
     * Test upsert with no key specified
     *
     * @depends testConnect
     */
     /*
    function testNoKeyUpsert($b) {
        $this->wrapException(function() use($b) {
            $b->upsert('', 'joe');
        }, 'CouchbaseException', COUCHBASE_EINVAL);
    }
    */

    /**
     * @test
     * Test all option values to make sure they save/load
     * We open a new bucket for this test to make sure our settings
     *   changes do not affect later tests
     */
    function testOptionVals() {
        $h = new CouchbaseCluster($this->testDsn);
        $b = $h->openBucket();
        
        $checkVal = 243;
        
        $b->operationTimeout = $checkVal;
        $b->viewTimeout = $checkVal;
        $b->durabilityInterval = $checkVal;
        $b->durabilityTimeout = $checkVal;
        $b->httpTimeout = $checkVal;
        $b->configTimeout = $checkVal;
        $b->configDelay = $checkVal;
        $b->configNodeTimeout = $checkVal;
        $b->htconfigIdleTimeout = $checkVal;
        
        $this->assertEquals($b->operationTimeout, $checkVal);
        $this->assertEquals($b->viewTimeout, $checkVal);
        $this->assertEquals($b->durabilityInterval, $checkVal);
        $this->assertEquals($b->durabilityTimeout, $checkVal);
        $this->assertEquals($b->httpTimeout, $checkVal);
        $this->assertEquals($b->configTimeout, $checkVal);
        $this->assertEquals($b->configDelay, $checkVal);
        $this->assertEquals($b->configNodeTimeout, $checkVal);
        $this->assertEquals($b->htconfigIdleTimeout, $checkVal);
    }
    
    /**
     * @test
     * Test all option values to make sure they save/load
     * We open a new bucket for this test to make sure our settings
     *   changes do not affect later tests
     */
    function testConfigCache() {
        $this->markTestSkipped(
              'Configuration cache is not currently behaving.'
            );
            
        $key = $this->makeKey('ccache');
    
        $h = new CouchbaseCluster(
            $this->testDsn . '?config_cache=./test.cache');
        $b = $h->openBucket();
        $b->upsert($key, 'yes');
        
        $h2 = new CouchbaseCluster(
            $this->testDsn . '?config_cache=./test.cache');
        $b2 = $h2->openBucket();
        $res = $b2->get($key);

        $this->assertValidMetaDoc($res, 'value', 'cas', 'flags');
        $this->assertEquals($res->value, 'yes');
    }

}
